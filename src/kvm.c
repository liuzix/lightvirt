#include "kvm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

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

mem_t *vm_map_guest_physical(void *host_vaddr, addr_t guest_paddr, size_t len)
{
	mem_t *ret = malloc(sizeof(mem_t));
	int slot = slot_find_first_available();

	if (slot < 0) goto failed;

	ret->slot = slot;
	ret->valid = 1;
	
	struct kvm_userspace_memory_region region;
	
	region.slot = ret->slot;
	region.flag = 0;
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

void vcpu_init(vm_t *vm, vcput_t *vcpu);
{
	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
 	if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
		exit(EXIT_FAILURE);
	}
}
