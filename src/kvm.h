#ifndef KVM_H
#define KVM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

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
	struct kvm_run *kvm_run;
};

typedef struct kvm_vcpu vcpu_t;

typedef uint64_t addr_t;

void vm_init(vm_t *vm);

mem_t *vm_map_guest_physical(void *host_vaddr, addr_t guest_paddr, size_t len);

void vcpu_init(vm_t *vm, vcput_t *vcpu);

#ifdef __cplusplus
}
#endif

#endif
