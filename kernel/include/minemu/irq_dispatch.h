#ifndef MINEMU_IRQ_DISPATCH_H
#define MINEMU_IRQ_DISPATCH_H

#include <stdint.h>

/* A handler for one interrupt source. Runs in IRQ context. */
typedef void (*irq_handler_t)(void);

/* Associate a handler with an interrupt source ID (e.g. MINEMU_IRQ_UART0).
   Call this during init, before unmasking IRQs at the CPU. */
void minemu_irq_register(uint32_t source, irq_handler_t handler);

#endif