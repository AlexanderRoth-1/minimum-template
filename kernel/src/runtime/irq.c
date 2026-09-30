#include <stddef.h>
#include <stdint.h>
#include <minemu/platform.h>
#include <minemu/trap.h>
#include <minemu/irq.h>
#include <minemu/uart.h>
typedef void (*irq_handler_t)(void);

#define IRQ_HANDLER_COUNT 32
static irq_handler_t irq_handlers[IRQ_HANDLER_COUNT];

void minemu_irq_register(uint32_t source, irq_handler_t handler) {
    if (source < IRQ_HANDLER_COUNT) {
        irq_handlers[source] = handler;
    }
}

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    
    uint32_t source = (uint32_t)frame->exception_id;

    if (source < IRQ_HANDLER_COUNT && irq_handlers[source] != NULL) {
        irq_handlers[source]();
    }

    MINEMU_INTERRUPT->eoi = source;
    return frame;
}