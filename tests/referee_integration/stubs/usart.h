#ifndef TEST_USART_H
#define TEST_USART_H

#include <stdint.h>

typedef struct
{
    uint32_t marker;
} UART_HandleTypeDef;

uint32_t HAL_GetTick(void);

#endif
