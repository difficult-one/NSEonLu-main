#include "main.h"
#include "bsp_uart_log.h"

#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

static int failures;
static HAL_StatusTypeDef init_status = HAL_OK;
static HAL_StatusTypeDef dma_status = HAL_OK;
static unsigned init_calls;
static unsigned dma_calls;
static const uint8_t *dma_data;
static unsigned dma_length;
static uint32_t primask;

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { printf("FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } \
} while (0)
#define ASSERT_EQ_UINT(expected, actual) do { \
    unsigned e = (expected), a = (actual); \
    if (e != a) { printf("FAIL:%s:%d: expected %u, got %u\n", __FILE__, __LINE__, e, a); ++failures; } \
} while (0)

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *handle)
{
    (void)handle;
    ++init_calls;
    return init_status;
}

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *handle,
                                        const uint8_t *data,
                                        uint16_t length)
{
    (void)handle;
    ++dma_calls;
    dma_data = data;
    dma_length = length;
    return dma_status;
}

uint32_t __get_PRIMASK(void) { return primask; }
void __set_PRIMASK(uint32_t value) { primask = value; }
void __disable_irq(void) { primask = 1U; }

static void Reset(void)
{
    init_status = HAL_OK;
    dma_status = HAL_OK;
    init_calls = 0U;
    dma_calls = 0U;
    dma_data = NULL;
    dma_length = 0U;
    primask = 0U;
    memset(&huart1, 0, sizeof(huart1));
    BSPUARTLogInit();
    dma_calls = 0U;
    dma_data = NULL;
    dma_length = 0U;
}

static void TestInitSelects250000AndResetsDiagnostics(void)
{
    BSPUARTLogDiagnostics_s diagnostics;
    Reset();
    ASSERT_EQ_UINT(250000U, huart1.Init.BaudRate);
    BSPUARTLogGetDiagnostics(&diagnostics);
    ASSERT_EQ_UINT(0U, diagnostics.dropped_messages);
    ASSERT_EQ_UINT(0U, diagnostics.tx_errors);
}

static void TestFirstWriteStartsDmaAndPreservesBytes(void)
{
    static const char text[] = "abc";
    Reset();
    ASSERT_EQ_UINT(3U, BSPUARTLogWrite(text, 3U));
    ASSERT_EQ_UINT(1U, dma_calls);
    ASSERT_EQ_UINT(3U, dma_length);
    ASSERT_TRUE(memcmp(dma_data, text, 3U) == 0);
}

static void TestBusyWriteQueuesWithoutRestartingDma(void)
{
    static const char first[] = "abc";
    static const char second[] = "def";
    Reset();
    BSPUARTLogWrite(first, 3U);
    BSPUARTLogWrite(second, 3U);
    ASSERT_EQ_UINT(1U, dma_calls);
    HAL_UART_TxCpltCallback(&huart1);
    ASSERT_EQ_UINT(2U, dma_calls);
    ASSERT_EQ_UINT(3U, dma_length);
    ASSERT_TRUE(memcmp(dma_data, second, 3U) == 0);
}

static void TestCompletionDrainsQueuedAndWrappedSegmentsInOrder(void)
{
    static char first[2040];
    static char second[2040];
    static char third[100];
    Reset();
    memset(first, 'a', sizeof(first));
    memset(second, 'b', sizeof(second));
    memset(third, 'c', sizeof(third));
    BSPUARTLogWrite(first, sizeof(first));
    BSPUARTLogWrite(second, sizeof(second));
    HAL_UART_TxCpltCallback(&huart1);
    BSPUARTLogWrite(third, sizeof(third));
    HAL_UART_TxCpltCallback(&huart1);
    HAL_UART_TxCpltCallback(&huart1);
    ASSERT_EQ_UINT(4U, dma_calls);
    ASSERT_TRUE(dma_data != NULL);
}

static void TestOversizeOrFullQueueDropsWholeMessage(void)
{
    static uint8_t data[4096];
    BSPUARTLogDiagnostics_s diagnostics;
    Reset();
    ASSERT_EQ_UINT(0U, BSPUARTLogWrite(data, sizeof(data)));
    BSPUARTLogGetDiagnostics(&diagnostics);
    ASSERT_EQ_UINT(1U, diagnostics.dropped_messages);
}

static void TestForeignUartCompletionIsIgnored(void)
{
    UART_HandleTypeDef other;
    Reset();
    BSPUARTLogWrite("abc", 3U);
    HAL_UART_TxCpltCallback(&other);
    ASSERT_EQ_UINT(1U, dma_calls);
}

static void TestFullRetainedQueueRecoversWithoutCapturePolling(HAL_StatusTypeDef failure)
{
    static uint8_t data[4095];
    Reset();
    memset(data, 'q', sizeof(data));
    dma_status = failure;
    ASSERT_EQ_UINT(sizeof(data), BSPUARTLogWrite(data, sizeof(data)));
    ASSERT_EQ_UINT(1U, dma_calls);

    /* Ordinary firmware has no capture task to poll a failed DMA start. */
    dma_status = HAL_OK;
    ASSERT_EQ_UINT(0U, BSPUARTLogWrite("next", 4U));
    ASSERT_EQ_UINT(2U, dma_calls);
    ASSERT_EQ_UINT(sizeof(data), dma_length);
    ASSERT_TRUE(memcmp(dma_data, data, sizeof(data)) == 0);
    ASSERT_EQ_UINT(0U, primask);

    HAL_UART_TxCpltCallback(&huart1);
    ASSERT_TRUE(BSPUARTLogIsIdle());
    ASSERT_EQ_UINT(4U, BSPUARTLogWrite("next", 4U));
    /* The next message wraps across the ring boundary after 4095 bytes. */
    ASSERT_EQ_UINT(1U, dma_length);
    ASSERT_EQ_UINT('n', dma_data[0]);
    HAL_UART_TxCpltCallback(&huart1);
    ASSERT_EQ_UINT(3U, dma_length);
    ASSERT_TRUE(memcmp(dma_data, "ext", 3U) == 0);
    HAL_UART_TxCpltCallback(&huart1);
    ASSERT_TRUE(BSPUARTLogIsIdle());
}

static void TestHalInitAndDmaStartFailuresAreCounted(void)
{
    BSPUARTLogDiagnostics_s diagnostics;
    init_status = HAL_ERROR;
    BSPUARTLogInit();
    dma_status = HAL_ERROR;
    BSPUARTLogWrite("abc", 3U);
    BSPUARTLogGetDiagnostics(&diagnostics);
    ASSERT_EQ_UINT(2U, diagnostics.tx_errors);
}

int main(void)
{
    TestInitSelects250000AndResetsDiagnostics();
    {
        Reset(); dma_status = HAL_ERROR;
        ASSERT_EQ_UINT(3, BSPUARTLogWrite("abc", 3));
        ASSERT_TRUE(!BSPUARTLogIsIdle());
        dma_status = HAL_OK;
        BSPUARTLogPoll();
        ASSERT_TRUE(memcmp(dma_data, "abc", 3) == 0);
        ASSERT_TRUE(!BSPUARTLogIsIdle());
        HAL_UART_TxCpltCallback(&huart1);
        ASSERT_TRUE(BSPUARTLogIsIdle());
    }
    TestFirstWriteStartsDmaAndPreservesBytes();
    TestBusyWriteQueuesWithoutRestartingDma();
    TestCompletionDrainsQueuedAndWrappedSegmentsInOrder();
    TestOversizeOrFullQueueDropsWholeMessage();
    TestForeignUartCompletionIsIgnored();
    TestFullRetainedQueueRecoversWithoutCapturePolling(HAL_ERROR);
    TestFullRetainedQueueRecoversWithoutCapturePolling(HAL_BUSY);
    TestHalInitAndDmaStartFailuresAreCounted();
    if (failures != 0)
        return 1;
    puts("PASS: uart log tests");
    return 0;
}
