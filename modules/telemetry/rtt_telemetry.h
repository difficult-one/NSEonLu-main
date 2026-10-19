#ifndef RTT_TELEMETRY_H
#define RTT_TELEMETRY_H

#include "power_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 0: standard 17-column control CSV; 1: append 9 calibration columns. */
#ifndef CHASSIS_POWER_CALIBRATION_ENABLE
#define CHASSIS_POWER_CALIBRATION_ENABLE 0
#endif

#if CHASSIS_POWER_CALIBRATION_ENABLE != 0 && CHASSIS_POWER_CALIBRATION_ENABLE != 1
#error CHASSIS_POWER_CALIBRATION_ENABLE must be 0 or 1
#endif

#define RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH 320U //预留的缓冲区大小

typedef struct
{
    float nominal_budget_w;
    float target_power_w;
    float corrected_budget_w;
    float filtered_power_w;
    float error_w;
    float correction_w;
    uint8_t outer_mode;
} ChassisPowerControlTelemetry_s;

/* Always formats the standard 17 columns, regardless of the writer mode. */
size_t RTTTelemetryFormatChassisPowerCsv(
    const ChassisPowerOutput_s *power,
    const ChassisPowerTelemetry_s *motor,
    const ChassisPowerControlTelemetry_s *control,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms,
    char *buffer,
    size_t buffer_size);

void RTTTelemetryWriteChassisPowerHeader(void);

void RTTTelemetryWriteChassisPowerRow(
    const ChassisPowerOutput_s *power,
    const ChassisPowerTelemetry_s *motor,
    const ChassisPowerControlTelemetry_s *control,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms);

#endif // RTT_TELEMETRY_H
