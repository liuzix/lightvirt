#include <iostream>
#include <deque>
#include "kvm.h"
#include "log.hpp"
#include "memory.hpp"

std::shared_ptr<spdlog::logger> console = spdlog::stdout_color_mt("console");

int main(int argc, char **argv)
{
	log_init();
	vm_t vm;
	vm_init(&vm);
	
	MemoryPool memoryPool(&vm, 0x0, 1 << 30);

	std::deque<addr_t> addrs;

	for (int i = 0; i < 10; i++)
		addrs.push_back(memoryPool.getPhysicalMemoryBlock(PAGE_SIZE));

	for (int i = 1; i < 7; i++)
		memoryPool.freePhysicalMemoryBlock(addrs[i], PAGE_SIZE);

	for (int i = 0; i < 10; i++)
		memoryPool.getPhysicalMemoryBlock(PAGE_SIZE * 2);


	vcpu_t *vcpu = vcpu_init(&vm);

	VCPU_REG(vcpu, rax) = 1000;

	//for (;;) {
		vcpu_run(vcpu);
	//}
	vcpu_destroy(vcpu);
	return 0;
}
