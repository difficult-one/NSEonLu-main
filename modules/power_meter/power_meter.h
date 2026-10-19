#ifndef POWER_METER_H
#define POWER_METER_H

#include "bsp_can.h"
#include "daemon.h"
#include "power_meter_protocol.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool online;
    float shunt_voltage_v;
    float bus_voltage_v;
    float current_a;
    float power_w;
    uint32_t last_rx_tick_ms;
} PowerMeterStatus_s;

typedef struct
{
    CANInstance *can_ins;
    DaemonInstance *daemon;
    PowerMeterStatus_s status;
    uint32_t offline_timeout_ms;
    volatile uint32_t received_frame_count;
    volatile uint32_t rejected_frame_count;
    volatile uint32_t last_rx_tick_ms;
} PowerMeterInstance;

typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
} PowerMeter_Init_Config_s;

bool PowerMeterInit(const PowerMeter_Init_Config_s *config);
bool PowerMeterGetStatus(PowerMeterStatus_s *status);

#endif
