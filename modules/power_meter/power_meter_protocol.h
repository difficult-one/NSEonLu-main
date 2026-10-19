#ifndef POWER_METER_PROTOCOL_H
#define POWER_METER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define POWER_METER_STATUS_CAN_ID 0x605U
#define POWER_METER_CAN_DLC 8U

typedef struct
{
    float shunt_voltage_v;
    float bus_voltage_v;
    float current_a;
    float power_w;
} PowerMeterMeasurement_s;

bool PowerMeterProtocolDecodeStatus(const uint8_t *frame,
                                    uint8_t dlc,
                                    PowerMeterMeasurement_s *measurement);

#endif
