#ifndef TEST_BSP_LOG_H
#define TEST_BSP_LOG_H

#include <stdarg.h>

static inline void TestLogDiscard(const char *format, ...)
{
    (void)format;
}

#define LOGERROR(...) TestLogDiscard(__VA_ARGS__)
#define LOGWARNING(...) TestLogDiscard(__VA_ARGS__)

unsigned BSPLogWrite(const void *data, unsigned length);
unsigned BSPLogWriteString(const char *text);

#endif
