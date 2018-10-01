#include <stddef.h>
#include <stdint.h>

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	__asm("hlt");
}
