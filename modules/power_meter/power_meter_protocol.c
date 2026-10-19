#include "power_meter_protocol.h"

#include <stddef.h>

static int32_t ReadI16BE(const uint8_t *bytes)
{
    int32_t value = (int32_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    if (value >= 0x8000)
        value -= 0x10000;
    return value;
}

bool PowerMeterProtocolDecodeStatus(const uint8_t *frame,
                                    uint8_t dlc,
                                    PowerMeterMeasurement_s *measurement)
{
    if (frame == NULL || measurement == NULL || dlc != POWER_METER_CAN_DLC)
        return false;

    const PowerMeterMeasurement_s decoded = {
        .shunt_voltage_v = (float)ReadI16BE(&frame[0]) / 1000.0f,
        .bus_voltage_v = (float)ReadI16BE(&frame[2]) / 1000.0f,
        .current_a = (float)ReadI16BE(&frame[4]) / 1000.0f,
        .power_w = (float)ReadI16BE(&frame[6]) / 100.0f,
    };
    *measurement = decoded;
    return true;
}
