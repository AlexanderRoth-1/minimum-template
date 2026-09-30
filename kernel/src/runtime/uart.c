#include <minemu/platform.h>
#include <minemu/irq.h>
#include <minemu/irq_dispatch.h>
#include <minemu/uart.h>

#define RX_BUF_SIZE 64   // power of two makes the wraparound mask easy

static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint32_t rx_head;   // written by the IRQ handler
static volatile uint32_t rx_tail;   // written by the main loop

void uart_putc(char c) {
    while ((MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) == 0) { }
    MINEMU_UART0->tx_data = (uint32_t)(uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

// Runs in IRQ context. Drain everything, or the interrupt refires.
static void uart0_irq_handler(void) {
    
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {   // check name in platform.h
        uint8_t b = (uint8_t)MINEMU_UART0->rx_data;
        uint32_t next = (rx_head + 1) & (RX_BUF_SIZE - 1);
        if (next != rx_tail) {          // not full
            rx_buf[rx_head] = b;
            rx_head = next;
        }                               // else drop the byte, but it's already been read
    }
}

// Runs in normal context, so it must guard the shared state.
bool uart_try_getc(uint8_t *out) {
    bool got = false;
    /* disable IRQs here, using the helper from irq.h */
    minemu_irq_disable();
    if (rx_tail != rx_head) {
        *out = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) & (RX_BUF_SIZE - 1);
        got = true;
    }
    /* re-enable IRQs here */
    minemu_irq_enable();
    return got;
}

void uart_init(void) {
    minemu_irq_register(MINEMU_IRQ_UART0, uart0_irq_handler);
    MINEMU_UART0->control |= 1u;                                    // RX interrupt on
    MINEMU_INTERRUPT->enable |= UINT32_C(1) << MINEMU_IRQ_UART0;    // controller forwards it
}