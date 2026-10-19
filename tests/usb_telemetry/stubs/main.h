#ifndef USB_TEST_MAIN_H
#define USB_TEST_MAIN_H
#include <stdint.h>
typedef struct { uint32_t marker; } UART_HandleTypeDef;
uint32_t HAL_GetTick(void);
#endif
