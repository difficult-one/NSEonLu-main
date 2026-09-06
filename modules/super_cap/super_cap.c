#include "super_cap.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

#define SUPERCAP_DEFAULT_OFFLINE_RELOAD_COUNT 20U

static void SuperCapOffline(void *owner)
{
    SuperCapInstance *instance = (SuperCapInstance *)owner;
    if (instance == NULL)
        return;

    taskENTER_CRITICAL();
    instance->status.online = false;
    taskEXIT_CRITICAL();
}

static void SuperCapRxCallback(CANInstance *can_instance)
{
    if (can_instance == NULL)
        return;

    SuperCapInstance *instance = (SuperCapInstance *)can_instance->id;
    SuperCapStatus_s decoded;
    if (instance == NULL ||
        !SuperCapProtocolDecodeStatus(can_instance->rx_buff,
                                      can_instance->rx_len,
                                      &decoded))
        return;

    instance->status = decoded;
    DaemonReload(instance->daemon);
}

SuperCapInstance *SuperCapInit(const SuperCap_Init_Config_s *config)
{
    if (config == NULL)
        return NULL;

    SuperCapInstance *instance = (SuperCapInstance *)malloc(sizeof(SuperCapInstance));
    if (instance == NULL)
        return NULL;
    memset(instance, 0, sizeof(*instance));

    const uint16_t reload_count = config->offline_reload_count == 0U
                                      ? SUPERCAP_DEFAULT_OFFLINE_RELOAD_COUNT
                                      : config->offline_reload_count;
    Daemon_Init_Config_s daemon_config = {
        .reload_count = reload_count,
        .init_count = reload_count,
        .callback = SuperCapOffline,
        .owner_id = instance,
    };
    instance->daemon = DaemonRegister(&daemon_config);
    if (instance->daemon == NULL)
    {
        free(instance);
        return NULL;
    }

    CAN_Init_Config_s can_config = config->can_config;
    can_config.can_module_callback = SuperCapRxCallback;
    can_config.id = instance;
    instance->can_ins = CANRegister(&can_config);
    if (instance->can_ins == NULL)
        return NULL;

    CANSetDLC(instance->can_ins, SUPERCAP_CAN_DLC);
    return instance;
}

bool SuperCapSendCommand(SuperCapInstance *instance,
                         const SuperCapCommand_s *command)
{
    if (instance == NULL || instance->can_ins == NULL ||
        !SuperCapProtocolEncodeCommand(command, instance->can_ins->tx_buff))
        return false;

    return CANTransmit(instance->can_ins, 1.0f) != 0U;
}

bool SuperCapGetStatus(SuperCapInstance *instance,
                       SuperCapStatus_s *status)
{
    if (instance == NULL || status == NULL || instance->daemon == NULL)
        return false;

    taskENTER_CRITICAL();
    *status = instance->status;
    status->online = status->online && DaemonIsOnline(instance->daemon) != 0U;
    taskEXIT_CRITICAL();
    return true;
}
