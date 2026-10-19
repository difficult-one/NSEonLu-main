#include "bsp_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int failures;
static unsigned captured_buffer_index;
static unsigned captured_length;
static char captured_data[128];

#define ASSERT_TRUE(condition)                                                   \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            printf("FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition);         \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

#define ASSERT_EQ_UINT(expected, actual)                                        \
    do                                                                           \
    {                                                                            \
        const unsigned expected_value = (expected);                              \
        const unsigned actual_value = (actual);                                  \
        if (expected_value != actual_value)                                      \
        {                                                                        \
            printf("FAIL:%s:%d: expected %u, got %u\n",                        \
                   __FILE__,                                                     \
                   __LINE__,                                                     \
                   expected_value,                                               \
                   actual_value);                                                \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

#define ASSERT_STREQ(expected, actual) ASSERT_TRUE(strcmp((expected), (actual)) == 0)

static void ResetCapture(void)
{
    captured_buffer_index = 99U;
    captured_length = 0U;
    memset(captured_data, 0, sizeof(captured_data));
}

void SEGGER_RTT_Init(void)
{
}

int SEGGER_RTT_vprintf(unsigned buffer_index, const char *format, va_list *args)
{
    captured_buffer_index = buffer_index;
    const int written = vsnprintf(captured_data, sizeof(captured_data), format, *args);
    captured_length = written > 0 ? (unsigned)written : 0U;
    return written;
}

unsigned SEGGER_RTT_Write(unsigned buffer_index, const void *data, unsigned length)
{
    captured_buffer_index = buffer_index;
    captured_length = length;
    memcpy(captured_data, data, length);
    return length;
}

unsigned SEGGER_RTT_WriteString(unsigned buffer_index, const char *text)
{
    captured_buffer_index = buffer_index;
    captured_length = (unsigned)strlen(text);
    memcpy(captured_data, text, captured_length + 1U);
    return captured_length;
}

static void TestRawWritePreservesEmbeddedNull(void)
{
    static const char data[] = {'a', '\0', 'b'};

    ResetCapture();
    ASSERT_EQ_UINT(3U, BSPLogWrite(data, 3U));
    ASSERT_EQ_UINT(0U, captured_buffer_index);
    ASSERT_EQ_UINT(3U, captured_length);
    ASSERT_TRUE(memcmp(data, captured_data, 3U) == 0);
}

static void TestStringWriteUsesDefaultBuffer(void)
{
    ResetCapture();
    ASSERT_EQ_UINT(5U, BSPLogWriteString("hello"));
    ASSERT_EQ_UINT(0U, captured_buffer_index);
    ASSERT_STREQ("hello", captured_data);
}

static void TestClearWritesTerminalControlSequence(void)
{
    ResetCapture();
    BSPLogClear();
    ASSERT_EQ_UINT(0U, captured_buffer_index);
    ASSERT_STREQ("  " RTT_CTRL_CLEAR, captured_data);
}

int main(void)
{
    TestRawWritePreservesEmbeddedNull();
    TestStringWriteUsesDefaultBuffer();
    TestClearWritesTerminalControlSequence();

    if (failures != 0)
        return 1;

    puts("PASS: bsp log tests");
    return 0;
}
