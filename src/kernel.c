#include "kernel.h"

#include <stddef.h>
#include <stdint.h>

void
__attribute__((section(".start")))
_start(void) {
	__asm("hlt");
}

void do_irq(struct idt_frame *frame)
{
	__asm("hlt");
}
