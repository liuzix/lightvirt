#include <iostream>
#include "kvm.h"


int main(int argc, char **argv)
{
	vm_t vm;
	vm_init(&vm);
	std::cout << "Hello World!\n" << std::endl;
	return 0;
}
