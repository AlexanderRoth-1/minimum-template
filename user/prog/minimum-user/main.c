#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "minemu/user_abi.h"

#define MSH_LINE_MAX 20

static char line[MSH_LINE_MAX];
static size_t line_len;
static bool line_overflow;

/* ---- output helpers (everything goes through ioctl) ---- */

static void out_write(const char *s, size_t n) {
    while (n > 0) {
        uint32_t chunk;
        if (n > MINEMU_IOCTL_MAX_TRANSFER) {
            chunk = MINEMU_IOCTL_MAX_TRANSFER;   /* 256 */
        } else {
            chunk = n;
        }
        if (ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
                  (void *)s, chunk) < 0) {
            return;
        }
        s += chunk;
        n -= chunk;
    }
}

static void out_putc(char c) { out_write(&c, 1); }

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') n++;
    return n;
}

static void out_puts(const char *s) { out_write(s, str_len(s)); }

/* ---- shell logic (same as the kernel msh.c) ---- */

static void msh_init(void) {
    line_len = 0;
    line_overflow = false;
    out_puts("msh> ");
}

static bool word_equals(const char *s, size_t n, const char *lit) {
    size_t i = 0;
    while (i < n && lit[i] != '\0') {
        if (s[i] != lit[i]) return false;
        i++;
    }
    return i == n && lit[i] == '\0';
}

static void msh_execute(const char *cmd, size_t len, bool overflow) {
    (void)overflow;
    size_t i = 0;

    while (i < len && cmd[i] == ' ') i++;      /* skip leading spaces */
    if (i == len) return;                      /* empty / all spaces */

    size_t start = i;                          /* first word */
    while (i < len && cmd[i] != ' ') i++;
    size_t word_len = i - start;

    if (word_equals(cmd + start, word_len, "echo")) {
        while (i < len && cmd[i] == ' ') i++;  /* collapse separator spaces */
        out_write(cmd + i, len - i);
        out_putc('\n');
    } else {
        out_puts("command not found: ");
        out_write(cmd + start, word_len);
        out_putc('\n');
    }
}

static void msh_feed(uint8_t b) {
    if (b == '\n') {
        msh_execute(line, line_len, line_overflow);
        line_len = 0;
        line_overflow = false;
        out_puts("msh> ");
    } else if (b == 0x08 || b == 0x7f) {       /* backspace */
        if (line_len > 0) line_len--;
    } else {
        if (line_len < MSH_LINE_MAX) {
            line[line_len++] = (char)b;
        } else {
            line_overflow = true;
        }
    }
}

/* ---- entry point called by the lib's startup code ---- */

int minemu_user_main(void) {
    msh_init();
    for (;;) {
        uint8_t b;
        if (ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ,
                  (void *)&b, (uint32_t)1) == 1) {
            msh_feed(b);
        }
    }
}