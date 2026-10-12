#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/trap.h"
#include "minemu/user_abi.h"
#include "minemu/mmu.h"
#include "minemu/user_mem.h"

static bool user_page_ok(uint32_t page_vaddr, bool need_write)
{
    if (page_vaddr < USER_IMAGE_BASE || page_vaddr >= USER_STACK_TOP)
        return false;

    uint32_t pte = user_page_table[(page_vaddr >> 12) & 1023U];
    uint32_t required = MINEMU_PTE_VALID | MINEMU_PTE_USER |
                        (need_write ? MINEMU_PTE_WRITABLE : MINEMU_PTE_READABLE);
    return (pte & required) == required;
}

static bool user_range_ok(uint32_t vaddr, uint32_t len, bool need_write)
{
    if (len == 0)
        return true;
    if (len - 1 > UINT32_MAX - vaddr)               /* wraparound */
        return false;

    uint32_t first = vaddr & MINEMU_PTE_PAGE_MASK;
    uint32_t last  = (vaddr + len - 1) & MINEMU_PTE_PAGE_MASK;

    for (uint32_t page = first; ; page += MINEMU_PAGE_SIZE) {
        if (!user_page_ok(page, need_write))
            return false;
        if (page == last)
            break;
    }
    return true;
}
static void fail(struct minemu_trap_frame *frame) {
    frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
}
static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *out = destination;
    const uint8_t *in = source;
    for (size_t index = 0; index < length; ++index) {
        out[index] = in[index];
    }
}
static void put_user_u32(uint32_t vaddr, uint32_t value)
{
    copy_bytes((void *)(uintptr_t)vaddr, &value, sizeof value);
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    /* 1. Sanity checks */
    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        (frame->spsr & CPSR_MODE_MASK) != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0 ||
        *(const uint32_t *)(uintptr_t)frame->fault_pc != A32_SVC_ZERO ||
        frame->r[7] != MINEMU_SYSCALL_IOCTL) {
        fail(frame);
        return frame;
    }

    uint32_t fd = frame->r[0], req = frame->r[1];
    uint32_t a0 = frame->r[2], a1 = frame->r[3];

    /* 2. Dispatch on request; validate fd and user memory BEFORE any effect */
    switch (req) {
    case MINEMU_IOCTL_UART_WRITE: {
        if ((fd != MINEMU_FD_UART_OUT && fd != MINEMU_FD_UART_DIAG) ||
            a1 > MINEMU_IOCTL_MAX_TRANSFER || !user_range_ok(a0, a1, false)) {
            fail(frame); break;
        }
        volatile struct minemu_uart_regs *uart =
            (fd == MINEMU_FD_UART_DIAG) ? MINEMU_UART1 : MINEMU_UART0;
        const uint8_t *src = (const uint8_t *)(uintptr_t)a0;
        for (uint32_t i = 0; i < a1; i++) {
            while (!(uart->status & MINEMU_UART_STATUS_TX_READY))
                ;
            uart->tx_data = src[i];
        }
        frame->r[0] = a1;
        break;
    }

    case MINEMU_IOCTL_UART_READ: {
        if (fd != MINEMU_FD_UART_IN ||
            a1 > MINEMU_IOCTL_MAX_TRANSFER || !user_range_ok(a0, a1, true)) {
            fail(frame); break;
        }
        uint8_t *dst = (uint8_t *)(uintptr_t)a0;
        uint32_t n = 0;
        while (n < a1 && (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY))
            dst[n++] = (uint8_t)(MINEMU_UART0->rx_data & 0xff);
        frame->r[0] = n;
        break;
    }

    case MINEMU_IOCTL_RNG_STATE:
        if (fd != MINEMU_FD_RNG || !user_range_ok(a0, 4, true)) {
            fail(frame); break;
        }
        put_user_u32(a0, MINEMU_RNG->state);
        frame->r[0] = 0;
        break;

    case MINEMU_IOCTL_RNG_NEXT:
        if (fd != MINEMU_FD_RNG || !user_range_ok(a0, 4, true)) {
            fail(frame); break;
        }
        put_user_u32(a0, MINEMU_RNG->data);
        frame->r[0] = 0;
        break;

    case MINEMU_IOCTL_RNG_SEED:
        if (fd != MINEMU_FD_RNG) { fail(frame); break; }
        MINEMU_RNG->seed = a0;
        frame->r[0] = 0;
        break;

    case MINEMU_IOCTL_TRACE_EVENT:
        if (fd != MINEMU_FD_TRACE) { fail(frame); break; }
        minemu_trace_event(a0);
        frame->r[0] = 0;
        break;

    default:
        fail(frame);
    }
    return frame;
}