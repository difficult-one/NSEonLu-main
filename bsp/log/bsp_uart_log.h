#ifndef BSP_UART_LOG_H
#define BSP_UART_LOG_H

/**
 * @file bsp_uart_log.h
 * @brief Non-blocking USART1 log transport.
 */

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t dropped_messages;
    uint32_t tx_errors;
} BSPUARTLogDiagnostics_s;

/** @brief Configure USART1 for the UART log transport and clear its queue. */
void BSPUARTLogInit(void);

/**
 * @brief Queue one complete log message for DMA transmission.
 * @return `length` when the complete message was queued, otherwise zero.
 */
unsigned BSPUARTLogWrite(const void *data, unsigned length);
/* Retry a retained DMA segment after HAL_BUSY/HAL_ERROR. */
void BSPUARTLogPoll(void);
bool BSPUARTLogIsIdle(void);

/** @brief Copy the current queue error counters to `out`. */
void BSPUARTLogGetDiagnostics(BSPUARTLogDiagnostics_s *out);

#endif
