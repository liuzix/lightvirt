#ifndef KVM_H
#define KVM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>
#include <linux/kvm.h>

#include "archflags.h"

#define SEGMENT_TYPE_CODE 11
#define SEGMENT_TYPE_DATA 3

#define VCPU_REG(v, r) (*(vcpu_access_gpregs(v, offsetof(struct kvm_regs, r))))
#define VCPU_SREG(v, r) (*(vcpu_access_sregs(v, offsetof(struct kvm_sregs, r))))

struct kvm_vm {
	/* the fd for /dev/kvm */
	int sys_fd;

	/* the fd for the VM */
	int fd;
};

typedef struct kvm_vm vm_t;

struct kvm_mem_region {
	uint32_t valid;
	uint32_t slot;
};

typedef struct kvm_mem_region mem_t;

struct kvm_vcpu {
	int fd;

	/* control fields to be mmap'd */
	struct kvm_run *kvm_run;

	/* control register file.
	 * they will be loaded on vmexits
	 * and write back on vmentries */
	struct kvm_sregs sregs;

	/* general purpose register file.
	 * same as above */
	struct kvm_regs regs;
};

typedef struct kvm_vcpu vcpu_t;

/* initialize the vm struct */
void vm_init(vm_t *vm);

/* for a given VM, map guest physical memory */
mem_t *vm_map_guest_physical(vm_t *vm, void *host_vaddr,
				addr_t guest_paddr, size_t len);

int vm_remap_guest_physical(vm_t *vm, mem_t *mem, void *host_vaddr,
		addr_t guest_paddr, size_t len);

void vm_unmap_guest_physical(vm_t *vm, mem_t *mem);

/* create a VCPU. It sets up segments, etc. */
vcpu_t *vcpu_init(vm_t *vm);

enum segment {CS, DS, ES, FS, GS, SS};

void vcpu_set_segment(vcpu_t *vcpu, enum segment segment,
			int selector, int type, int dpl);

static inline uint64_t *vcpu_access_gpregs(vcpu_t *vcpu, size_t offset)
{
	return (uint64_t *)(((char *)&vcpu->regs) + offset);
}

static inline uint64_t *vcpu_access_sregs(vcpu_t *vcpu, size_t offset)
{
	return (uint64_t *)(((char *)&vcpu->sregs) + offset);
}

enum vcpu_exit_reason {
	VCPU_HYPERCALL,
	VCPU_PAGEFAULT,
	VCPU_UD,
	VCPU_GP,
	VCPU_KVM_RUN_FAILED,
	VCPU_UNKNOWN
};

/* run a vcpu until exit */
enum vcpu_exit_reason vcpu_run(vcpu_t *cpu);

void vcpu_destroy(vcpu_t *vcpu);
#ifdef __cplusplus
}
#endif

#endif
