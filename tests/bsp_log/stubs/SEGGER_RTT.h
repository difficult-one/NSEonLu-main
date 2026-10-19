#ifndef TEST_SEGGER_RTT_H
#define TEST_SEGGER_RTT_H

#include <stdarg.h>

#define RTT_CTRL_CLEAR "\x1B[2J"
#define RTT_CTRL_RESET "\x1B[0m"
#define RTT_CTRL_TEXT_BRIGHT_GREEN "\x1B[92m"
#define RTT_CTRL_TEXT_BRIGHT_YELLOW "\x1B[93m"
#define RTT_CTRL_TEXT_BRIGHT_RED "\x1B[91m"

void SEGGER_RTT_Init(void);
int SEGGER_RTT_vprintf(unsigned buffer_index, const char *format, va_list *args);
unsigned SEGGER_RTT_Write(unsigned buffer_index, const void *data, unsigned length);
unsigned SEGGER_RTT_WriteString(unsigned buffer_index, const char *text);

#endif
