#include "bsp_log.h"

#include "board_config.h"
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"
#include "bsp_uart_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BSP_LOG_FORMAT_BUFFER_SIZE 384U // 384字节的格式化缓冲区,足够打印一行日志,超过则截断

static char format_buffer[BSP_LOG_FORMAT_BUFFER_SIZE];
static volatile unsigned char format_busy;

#if UART1_LOG_ENABLED
static unsigned StripAnsiSequences(char *text, unsigned length)
{
    unsigned read_index = 0U;
    unsigned write_index = 0U;

    while (read_index < length)
    {
        if ((unsigned char)text[read_index] == 0x1BU &&
            read_index + 1U < length && text[read_index + 1U] == '[')
        {
            read_index += 2U;
            while (read_index < length &&
                   ((unsigned char)text[read_index] < 0x40U ||
                    (unsigned char)text[read_index] > 0x7EU))
                ++read_index;
            if (read_index < length)
                ++read_index;
            continue;
        }
        text[write_index++] = text[read_index++];
    }
    return write_index;
}
#endif

static unsigned RouteRaw(const void *data, unsigned length)
{
    unsigned accepted = length;

#if UART1_RTT_ENABLED
    if (SEGGER_RTT_Write(BUFFER_INDEX, data, length) != length)
        accepted = 0U;
#endif
#if UART1_LOG_ENABLED
    if (BSPUARTLogWrite(data, length) != length)
        accepted = 0U;
#endif
    return accepted;
}

static int RouteFormatted(char *text, unsigned length)
{
    int accepted = 1;

#if UART1_RTT_ENABLED
    if (SEGGER_RTT_Write(BUFFER_INDEX, text, length) != length)
        accepted = 0;
#endif
#if UART1_LOG_ENABLED
    const unsigned uart_length = StripAnsiSequences(text, length);
    if (BSPUARTLogWrite(text, uart_length) != uart_length)
        accepted = 0;
#endif
    return accepted ? (int)length : 0;
}

void BSPLogInit(void)
{
#if UART1_RTT_ENABLED
    SEGGER_RTT_Init();
#endif
#if UART1_LOG_ENABLED
    BSPUARTLogInit();
#endif
}

unsigned BSPLogWrite(const void *data, unsigned length)
{
    if (data == NULL || length == 0U)
        return 0U;
    return RouteRaw(data, length);
}

unsigned BSPLogWriteString(const char *text)
{
    if (text == NULL)
        return 0U;
    return BSPLogWrite(text, (unsigned)strlen(text));
}

void BSPLogClear(void)
{
    (void)BSPLogWriteString("  " RTT_CTRL_CLEAR);
}

int PrintLog(const char *fmt, ...)
{
    va_list args;
    int written;
    int result;

    if (fmt == NULL || __atomic_test_and_set(&format_busy, __ATOMIC_ACQUIRE))
        return 0;

    va_start(args, fmt);
    written = vsnprintf(format_buffer, sizeof(format_buffer), fmt, args);
    va_end(args);
    if (written < 0 || (unsigned)written >= sizeof(format_buffer))
        result = -1;
    else
        result = RouteFormatted(format_buffer, (unsigned)written);
    __atomic_clear(&format_busy, __ATOMIC_RELEASE);
    return result;
}

void Float2Str(char *str, float va)
{
    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - head) * 1000);
    head = abs(head);
    point = abs(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}
