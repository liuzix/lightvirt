#include "kvm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kvm.h>
#include "archflags.h"

#define KVM_DEBUG 1

static uint32_t max_slots;

static uint64_t *slot_bitmap;

#define ACCESS_SLOT(i) (1 & (slot_bitmap[i / 64] >> (uint64_t)(i % 64)))

static inline void slot_set(uint32_t i, int v)
{
	if (v) slot_bitmap[i / 64] |= (1ULL << (uint64_t)(i % 64));
	else slot_bitmap[i / 64] &= ~(1ULL << (uint64_t)(i % 64));
}

static int slot_find_first_available(void)
{
	// max_slots should be a multiple of 64
	uint32_t max_index = max_slots / 64;
	uint32_t i;

	for (i = 0; i < max_index; i++) {
		if (~slot_bitmap[i]) break;
	}

	if (i == max_index) {
		goto failed;
	}

	uint64_t qword = slot_bitmap[i];

	for (int j = 0; j < 64; j++) {
		if (1ULL & (qword >> (uint64_t)j))
			return j + 64 * i;
	}

failed:
	fprintf(stderr, "Error: out of memory slots\n");
	return -1;
}

static inline void slot_free(int index)
{
	assert(index < max_slots);
	assert(ACCESS_SLOT(index));

	slot_set(index, 0);
}

static void kvm_debug(const char *fmt, ...)
{
#if KVM_DEBUG == 1
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
#endif
}

void vm_init(vm_t *vm)
{
	vm->sys_fd = open("/dev/kvm", O_RDWR);
	if (vm->sys_fd < 0) {
		perror("open /dev/kvm");
		exit(EXIT_FAILURE);
	}

	int api_ver;

	api_ver = ioctl(vm->sys_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(EXIT_FAILURE);
	}

	if (api_ver != KVM_API_VERSION) {
		fprintf(stderr, "Got KVM api version %d, expected %d\n",
			api_ver, KVM_API_VERSION);
		exit(EXIT_FAILURE);
	}

	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(EXIT_FAILURE);
	}

	max_slots = ioctl(vm->sys_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_MEMSLOTS);
	if (max_slots < 0) {
		perror("KVM_CHECK_EXTENSION");
		exit(EXIT_FAILURE);
	} else {
		// round to multiple of 64
		max_slots &= ~63;
		kvm_debug("KVM: max_slots = %d\n", max_slots);
		slot_bitmap = malloc(max_slots / 8);
		memset(slot_bitmap, 0, max_slots / 8);
	}
}

mem_t *vm_map_guest_physical(vm_t *vm, void *host_vaddr, addr_t guest_paddr, size_t len)
{
	mem_t *ret = malloc(sizeof(mem_t));
	int slot = slot_find_first_available();

	if (slot < 0) goto failed;

	ret->slot = slot;
	ret->valid = 1;

	struct kvm_userspace_memory_region region;

	region.slot = ret->slot;
	region.flags = 0;
	region.guest_phys_addr = guest_paddr;
	region.userspace_addr = (uint64_t)host_vaddr;
	region.memory_size = len;
	region.memory_size = len;

	if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
		goto failed;
	}

	return ret;
failed:
	free(ret);
	return NULL;
}

void vm_unmap_guest_physical(vm_t *vm, mem_t *mem)
{
	assert(mem->valid == 1);
	slot_free(mem->slot);
	free(mem);
}

static void fill_segment(struct kvm_segment *segment, int selector,
				int type, int dpl)
{
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = selector,
		.present = 1,
		.type = type,
		.dpl = dpl,
		.db = 0,
		.s = 1,
		.l = 1,
		.g = 1,
	};

	*segment = seg;
}

void vcpu_set_segment(vcpu_t *vcpu, enum segment segment, int selector,
				int type, int dpl)
{
	struct kvm_segment *cur_seg = NULL;

	switch(segment){
	case CS:
		cur_seg = &vcpu->sregs.cs;
		break;
	case DS:
		cur_seg = &vcpu->sregs.ds;
		break;
	case ES:
		cur_seg = &vcpu->sregs.es;
		break;
	case FS:
		cur_seg = &vcpu->sregs.fs;
		break;
	case GS:
		cur_seg = &vcpu->sregs.gs;
		break;
	case SS:
		cur_seg = &vcpu->sregs.ss;
		break;
	default:
		assert(0);
	}

	fill_segment(cur_seg, selector, type, dpl);
}

static void __vcpu_load_regs(vcpu_t *vcpu)
{
	if (ioctl(vcpu->fd, KVM_GET_SREGS, &vcpu->sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(EXIT_FAILURE);
	}

	if (ioctl(vcpu->fd, KVM_GET_REGS, &vcpu->regs) < 0) {
		perror("KVM_GET_REGS");
		exit(EXIT_FAILURE);
	}

}

static void __vcpu_store_regs(vcpu_t *vcpu)
{
	if (ioctl(vcpu->fd, KVM_SET_SREGS, &vcpu->sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(EXIT_FAILURE);
	}

	if (ioctl(vcpu->fd, KVM_SET_REGS, &vcpu->regs) < 0) {
		perror("KVM_SET_REGS");
		exit(EXIT_FAILURE);
	}

}

/* sets up basic execution environment for long mode */
static void __vcpu_setup_long_mode(vcpu_t *vcpu)
{
	memset(&vcpu->sregs, 0, sizeof(vcpu->sregs));
	memset(&vcpu->regs, 0, sizeof(vcpu->regs));

	vcpu->sregs.cr4 = CR4_PAE;
	vcpu->sregs.cr0 = CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	vcpu->sregs.efer = EFER_SCE | EFER_LME | EFER_LMA;

	/* load the default segment registers
	 * might be overwritten when GDT is set up */
	vcpu_set_segment(vcpu, CS, 8, SEGMENT_TYPE_CODE, 0);
	vcpu_set_segment(vcpu, DS, 16, SEGMENT_TYPE_DATA, 0);
	vcpu_set_segment(vcpu, ES, 16, SEGMENT_TYPE_DATA, 0);
	vcpu_set_segment(vcpu, FS, 16, SEGMENT_TYPE_DATA, 0);
	vcpu_set_segment(vcpu, GS, 16, SEGMENT_TYPE_DATA, 0);
	vcpu_set_segment(vcpu, SS, 16, SEGMENT_TYPE_DATA, 0);

	/* bit 1 of rflags is reserved and has to be 1 */
	vcpu->regs.rflags = 1 << 1;
}

void vcpu_init(vm_t *vm, vcpu_t *vcpu)
{
	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
	if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
		exit(EXIT_FAILURE);
	}

	size_t vcpu_mmap_size = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);

	if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
		exit(EXIT_FAILURE);
	}

	vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, vcpu->fd, 0);
	if (vcpu->kvm_run <= 0) {
		perror("mmap vcpu");
		exit(EXIT_FAILURE);
	}

	__vcpu_setup_long_mode(vcpu);
}

enum vcpu_exit_reason vcpu_run(vcpu_t *vcpu)
{
	/* sync the userspace register file with the kernel */
	__vcpu_store_regs(vcpu);

	if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
		perror("KVM_RUN");
		return VCPU_KVM_RUN_FAILED;
	}

	__vcpu_load_regs(vcpu);

	uint32_t exit_reason = vcpu->kvm_run->exit_reason;

	switch (exit_reason) {
	case KVM_EXIT_HLT:
		kvm_debug("KVM: hypercall received\n");
		return VCPU_HYPERCALL;

	case KVM_EXIT_EXCEPTION:
		{
			uint32_t exception_vector = vcpu->kvm_run->ex.exception;

			kvm_debug("Exception: %d\n", exception_vector);
			if (exception_vector == 14) {
				return VCPU_PAGEFAULT;
			}

			if (exception_vector == 13) {
				return VCPU_GP;
			}

			if (exception_vector == 6) {
				return VCPU_UD;
			}

			kvm_debug("Unhandled exception: %d\n", exception_vector);
			return VCPU_UNKNOWN;
		}
	case KVM_EXIT_UNKNOWN:
		{
			uint64_t hardware_exit_reason =
				vcpu->kvm_run->hw.hardware_exit_reason;
			kvm_debug("Hardware exit reason: 0x%llx\n", hardware_exit_reason);
		}
	case KVM_EXIT_FAIL_ENTRY: 
		{
			uint64_t hardware_entry_failure_reason =
				vcpu->kvm_run->fail_entry.hardware_entry_failure_reason;
			kvm_debug("Hardware failed entry reason: 0x%llx\n", hardware_entry_failure_reason);
		}
	default:
		kvm_debug("KVM: unknown exit reason %x\n", exit_reason);
		return VCPU_UNKNOWN;
	}
}
