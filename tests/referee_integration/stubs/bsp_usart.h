#ifndef TEST_BSP_USART_H
#define TEST_BSP_USART_H

#include "usart.h"

#include <stdint.h>

#define USART_RXBUFF_LIMIT 256U

typedef void (*usart_module_callback)(void);

typedef enum
{
    USART_TRANSFER_NONE = 0,
    USART_TRANSFER_BLOCKING,
    USART_TRANSFER_IT,
    USART_TRANSFER_DMA,
} USART_TRANSFER_MODE;

typedef struct
{
    uint8_t recv_buff[USART_RXBUFF_LIMIT];
    uint8_t recv_buff_size;
    uint16_t recv_data_len;
    UART_HandleTypeDef *usart_handle;
    usart_module_callback module_callback;
} USARTInstance;

typedef struct
{
    uint8_t recv_buff_size;
    UART_HandleTypeDef *usart_handle;
    usart_module_callback module_callback;
} USART_Init_Config_s;

USARTInstance *USARTRegister(USART_Init_Config_s *init_config);
void USARTServiceInit(USARTInstance *instance);
void USARTSend(USARTInstance *instance,
               uint8_t *send_buf,
               uint16_t send_size,
               USART_TRANSFER_MODE mode);

#endif
