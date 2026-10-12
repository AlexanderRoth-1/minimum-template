   #include <stdint.h>
   #include "minemu/user_abi.h"

   static const char msg[] = "Hello, world!\n";

   int minemu_user_main(void) {
       ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
             (void *)msg, (uint32_t)(sizeof(msg) - 1));
       for (;;) { }
   }
