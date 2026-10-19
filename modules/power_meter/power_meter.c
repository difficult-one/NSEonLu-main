#include "power_meter.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

#define POWER_METER_DEFAULT_OFFLINE_RELOAD_COUNT 10U    // 默认离线重载值,即100ms内没有接收到数据则认为离线
#define POWER_METER_DAEMON_PERIOD_MS 10U    // 守护进程周期,即每10ms调用一次守护进程的回调函数

static PowerMeterInstance *power_meter;
static bool power_meter_init_started;

static bool PowerMeterIsFresh(const PowerMeterInstance *instance,
                              uint32_t now_ms)
{
    return instance->status.online &&
           (uint32_t)(now_ms - instance->last_rx_tick_ms) <
               instance->offline_timeout_ms;
}

static void PowerMeterOffline(void *owner)
{
    PowerMeterInstance *instance = (PowerMeterInstance *)owner;
    if (instance == NULL)
        return;

    taskENTER_CRITICAL();
    if (!PowerMeterIsFresh(instance, HAL_GetTick()))
        instance->status.online = false;
    taskEXIT_CRITICAL();
}

static void PowerMeterRxCallback(CANInstance *can_instance)
{
    if (can_instance == NULL)
        return;

    PowerMeterInstance *instance = (PowerMeterInstance *)can_instance->id;
    if (instance == NULL)
        return;

    PowerMeterMeasurement_s decoded;
    if (!PowerMeterProtocolDecodeStatus(can_instance->rx_buff,
                                        can_instance->rx_len,
                                        &decoded))
    {
        instance->rejected_frame_count++;
        return;
    }

    instance->status.online = true;
    instance->status.shunt_voltage_v = decoded.shunt_voltage_v;
    instance->status.bus_voltage_v = decoded.bus_voltage_v;
    instance->status.current_a = decoded.current_a;
    instance->status.power_w = decoded.power_w;
    instance->received_frame_count++;
    instance->last_rx_tick_ms = HAL_GetTick();
    DaemonReload(instance->daemon);
}

bool PowerMeterInit(const PowerMeter_Init_Config_s *config)
{
    if (config == NULL || power_meter_init_started)
        return false;

    PowerMeterInstance *instance =
        (PowerMeterInstance *)malloc(sizeof(PowerMeterInstance));
    if (instance == NULL)
        return false;
    memset(instance, 0, sizeof(*instance));

    const uint16_t reload_count =
        config->offline_reload_count == 0U
            ? POWER_METER_DEFAULT_OFFLINE_RELOAD_COUNT
            : config->offline_reload_count;
    instance->offline_timeout_ms =
        (uint32_t)reload_count * POWER_METER_DAEMON_PERIOD_MS;
    Daemon_Init_Config_s daemon_config = {
        .reload_count = reload_count,
        .init_count = reload_count,
        .callback = PowerMeterOffline,
        .owner_id = instance,
    };
    instance->daemon = DaemonRegister(&daemon_config);
    if (instance->daemon == NULL)
    {
        free(instance);
        return false;
    }
    power_meter_init_started = true;

    CAN_Init_Config_s can_config = config->can_config;
    can_config.can_module_callback = PowerMeterRxCallback;
    can_config.id = instance;
    instance->can_ins = CANRegister(&can_config);
    if (instance->can_ins == NULL)
        return false;

    CANSetDLC(instance->can_ins, POWER_METER_CAN_DLC);
    power_meter = instance;
    return true;
}

bool PowerMeterGetStatus(PowerMeterStatus_s *status)
{
    if (power_meter == NULL || status == NULL || power_meter->daemon == NULL)
        return false;

    taskENTER_CRITICAL();
    *status = power_meter->status;
    status->last_rx_tick_ms = power_meter->last_rx_tick_ms;
    status->online = PowerMeterIsFresh(power_meter, HAL_GetTick());
    taskEXIT_CRITICAL();
    return true;
}
