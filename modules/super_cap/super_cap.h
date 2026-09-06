#ifndef SUPER_CAP_H
#define SUPER_CAP_H

#include "bsp_can.h"
#include "daemon.h"
#include "super_cap_protocol.h"

typedef struct
{
    CANInstance *can_ins;
    DaemonInstance *daemon;
    SuperCapStatus_s status;
} SuperCapInstance;

typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
} SuperCap_Init_Config_s;

SuperCapInstance *SuperCapInit(const SuperCap_Init_Config_s *config);
bool SuperCapSendCommand(SuperCapInstance *instance,
                         const SuperCapCommand_s *command);
bool SuperCapGetStatus(SuperCapInstance *instance,
                       SuperCapStatus_s *status);

#endif
