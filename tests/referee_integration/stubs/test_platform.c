#include "test_platform.h"

#include "cmsis_os.h"
#include "daemon.h"

#include <string.h>

static USARTInstance usart_instance;
static DaemonInstance daemon_instance;
static uint32_t tick_ms;
static uint32_t tick_calls_inside_critical;
static uint32_t tick_calls_outside_critical;
static uint32_t daemon_reload_count;
static int critical_depth;

void TestPlatformReset(void)
{
    memset(&usart_instance, 0, sizeof(usart_instance));
    memset(&daemon_instance, 0, sizeof(daemon_instance));
    tick_ms = 0U;
    tick_calls_inside_critical = 0U;
    tick_calls_outside_critical = 0U;
    daemon_reload_count = 0U;
    critical_depth = 0;
}

void TestPlatformSetTick(uint32_t value)
{
    tick_ms = value;
}

void TestPlatformResetTickObservations(void)
{
    tick_calls_inside_critical = 0U;
    tick_calls_outside_critical = 0U;
}

bool TestPlatformFeed(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0U ||
        length > sizeof(usart_instance.recv_buff) ||
        usart_instance.module_callback == NULL)
    {
        return false;
    }

    memcpy(usart_instance.recv_buff, data, length);
    usart_instance.recv_data_len = (uint16_t)length;
    usart_instance.module_callback();
    memset(usart_instance.recv_buff, 0, length);
    usart_instance.recv_data_len = 0U;
    return true;
}

USARTInstance *TestPlatformGetUSART(void)
{
    return &usart_instance;
}

uint32_t TestPlatformTickCallsInsideCritical(void)
{
    return tick_calls_inside_critical;
}

uint32_t TestPlatformTickCallsOutsideCritical(void)
{
    return tick_calls_outside_critical;
}

uint32_t TestPlatformDaemonReloadCount(void)
{
    return daemon_reload_count;
}

int TestPlatformCriticalDepth(void)
{
    return critical_depth;
}

void TestPlatformEnterCritical(void)
{
    ++critical_depth;
}

void TestPlatformExitCritical(void)
{
    --critical_depth;
}

uint32_t HAL_GetTick(void)
{
    if (critical_depth > 0)
        ++tick_calls_inside_critical;
    else
        ++tick_calls_outside_critical;
    return tick_ms;
}

USARTInstance *USARTRegister(USART_Init_Config_s *init_config)
{
    memset(&usart_instance, 0, sizeof(usart_instance));
    usart_instance.recv_buff_size = init_config->recv_buff_size;
    usart_instance.usart_handle = init_config->usart_handle;
    usart_instance.module_callback = init_config->module_callback;
    return &usart_instance;
}

void USARTServiceInit(USARTInstance *instance)
{
    (void)instance;
}

void USARTSend(USARTInstance *instance,
               uint8_t *send_buf,
               uint16_t send_size,
               USART_TRANSFER_MODE mode)
{
    (void)instance;
    (void)send_buf;
    (void)send_size;
    (void)mode;
}

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config)
{
    daemon_instance.reload_count = config->reload_count;
    daemon_instance.callback = config->callback;
    daemon_instance.owner_id = config->owner_id;
    return &daemon_instance;
}

void DaemonReload(DaemonInstance *instance)
{
    if (instance != NULL)
        instance->temp_count = instance->reload_count;
    ++daemon_reload_count;
}

void osDelay(uint32_t milliseconds)
{
    (void)milliseconds;
}
