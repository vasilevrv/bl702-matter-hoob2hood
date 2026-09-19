#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int hood_uart_init(void);
int hood_uart_write(uint8_t byte);

#ifdef __cplusplus
}
#endif
