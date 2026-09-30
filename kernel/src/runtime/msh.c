#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <minemu/msh.h>
#include <minemu/uart.h>

#define LINE_MAX 20

static char line[LINE_MAX];
static size_t line_len;
static bool line_overflow;

void msh_init(void) {
    line_len = 0;
    line_overflow = false;
    uart_puts("msh> ");
}
static bool word_equals(const char *s, size_t n, const char *lit) {
    size_t i = 0;
    while (i < n && lit[i] != '\0') {
        if (s[i] != lit[i]) return false;
        i++;
    }
    return i == n && lit[i] == '\0';   // same length and all chars matched
}

static void msh_execute(const char *cmd, size_t len, bool overflow) {
    (void)overflow;                    // decide what to do with this later
    size_t i = 0;

    while (i < len && cmd[i] == ' ') i++;      // 1. skip leading spaces
    if (i == len) return;                      // 2. empty / all spaces

    size_t start = i;                          // 3. find the end of the first word
    while (i < len && cmd[i] != ' ') i++;
    size_t word_len = i - start;

    if (word_equals(cmd + start, word_len, "echo")) {   // 4/5
        while (i < len && cmd[i] == ' ') i++;           // skip separator spaces
        for (; i < len; i++) uart_putc(cmd[i]);
        uart_putc('\n');
    } else {                                            // 6
        uart_puts("command not found: ");
        for (size_t j = 0; j < word_len; j++) uart_putc(cmd[start + j]);
        uart_putc('\n');
    }
}
void msh_feed(uint8_t b) {
    if (b == '\n') {
        // line is complete: run it, then print the next prompt
        msh_execute(line, line_len, line_overflow);
        line_len = 0;
        line_overflow = false;
        uart_puts("msh> ");
    } else if (b == 0x08 || b == 0x7f) {
        // backspace: ignore if empty, otherwise drop the last byte
        if (line_len > 0) {
            line_len--;
        }
    } else {
        // ordinary character: store it if there's room
        if (line_len < LINE_MAX) {
            line[line_len++] = (char)b;
        } else {
            line_overflow = true;
        }
    }
}