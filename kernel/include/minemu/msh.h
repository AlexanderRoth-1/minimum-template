#ifndef MINEMU_MSH_H
#define MINEMU_MSH_H

#include <stdint.h>

void msh_init(void);          // prints the first prompt, resets state
void msh_feed(uint8_t b);     // process one received byte

#endif