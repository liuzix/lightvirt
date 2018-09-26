#include <iostream>
#include "kvm.h"


int main(int argc, char **argv)
{
	vm_t vm;
	vm_init(&vm);

	vcpu_t vcpu;
	vcpu_init(&vm, &vcpu);

	VCPU_REG(&vcpu, rax) = 1000;
	std::cout << "Hello World!\n" << std::endl;
	return 0;
}
