#include "rtt_telemetry.h"

#include "bsp_log.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>

static const char chassis_power_header[] =
    "tick,nominal_budget_cw,target_power_cw,corrected_budget_cw,"
    "effective_limit_cw,p_unlimited_cw,p_limited_cw,p_meter_cw,"
    "p_filtered_cw,error_cw,correction_cw,min_scale_pm,limited_mask,"
    "outer_mode,meter_online,meter_age_ms,motor_enabled_mask"
#if CHASSIS_POWER_CALIBRATION_ENABLE
    ",i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,"
    "n_lf_rpm,n_rf_rpm,n_lb_rpm,n_rb_rpm,meter_tick_ms"
#endif
    "\r\n";

static char chassis_power_row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

static int32_t RoundTelemetryValue(float value)
{
    if (!isfinite(value))
        return 0;
    if (value >= (float)INT32_MAX)
        return INT32_MAX;
    if (value <= (float)INT32_MIN)
        return INT32_MIN;
    return (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static int32_t PowerToCentiwatts(float power_w)
{
    if (!isfinite(power_w))
        return 0;
    if (power_w >= (float)INT32_MAX / 100.0f)
        return INT32_MAX;
    if (power_w <= (float)INT32_MIN / 100.0f)
        return INT32_MIN;
    return RoundTelemetryValue(power_w * 100.0f);
}

static float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static void SummarizePowerOutput(const ChassisPowerOutput_s *power,
                                 float *effective_limit_w,
                                 float *predicted_unlimited_power_w,
                                 int32_t *minimum_scale_permille,
                                 uint8_t *limited_mask)
{
    *effective_limit_w = 0.0f;
    *predicted_unlimited_power_w = 0.0f;
    *minimum_scale_permille = 0;
    *limited_mask = 0U;

    if (power->status != POWER_MODEL_STATUS_OK)
        return;

    float minimum_scale = 1.0f;
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        *predicted_unlimited_power_w +=
            power->predicted_unlimited_power_w[i];

        float scale = power->current_scale[i];
        if (!isfinite(scale))
            scale = 0.0f;
        scale = ClampFloat(scale, 0.0f, 1.0f);
        if (scale < minimum_scale)
            minimum_scale = scale;
        if (scale < 1.0f)
            *limited_mask |= (uint8_t)(1U << i);
    }

    *effective_limit_w = power->effective_limit_w;
    *minimum_scale_permille = RoundTelemetryValue(minimum_scale * 1000.0f);
}

size_t RTTTelemetryFormatChassisPowerCsv(
    const ChassisPowerOutput_s *power,
    const ChassisPowerTelemetry_s *motor,
    const ChassisPowerControlTelemetry_s *control,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms,
    char *buffer,
    size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0U)
        return 0U;
    buffer[0] = '\0';
    if (power == NULL || motor == NULL || control == NULL)
        return 0U;

    float effective_limit_w;
    float predicted_unlimited_power_w;
    int32_t minimum_scale_permille;
    uint8_t limited_mask;
    SummarizePowerOutput(power,
                         &effective_limit_w,
                         &predicted_unlimited_power_w,
                         &minimum_scale_permille,
                         &limited_mask);

    const uint32_t meter_age_ms =
        meter_tick_ms == 0U ? 0U : tick_ms - meter_tick_ms;

    const int written = snprintf(
        buffer,
        buffer_size,
        "%lu,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%lu,%u\r\n",
        (unsigned long)tick_ms,
        (long)PowerToCentiwatts(control->nominal_budget_w),
        (long)PowerToCentiwatts(control->target_power_w),
        (long)PowerToCentiwatts(control->corrected_budget_w),
        (long)PowerToCentiwatts(effective_limit_w),
        (long)PowerToCentiwatts(predicted_unlimited_power_w),
        (long)PowerToCentiwatts(motor->predicted_total_power_w),
        (long)PowerToCentiwatts(measured_chassis_power_w),
        (long)PowerToCentiwatts(control->filtered_power_w),
        (long)PowerToCentiwatts(control->error_w),
        (long)PowerToCentiwatts(control->correction_w),
        (long)minimum_scale_permille,
        (unsigned)(limited_mask & 0x0FU),
        (unsigned)control->outer_mode,
        meter_online ? 1U : 0U,
        (unsigned long)meter_age_ms,
        (unsigned)(motor->motor_enabled_mask & 0x0FU));

    if (written < 0 || (size_t)written >= buffer_size)
    {
        buffer[0] = '\0';
        return 0U;
    }
    return (size_t)written;
}

#if CHASSIS_POWER_CALIBRATION_ENABLE
static size_t FormatChassisPowerCalibrationCsv(
    const ChassisPowerOutput_s *power,
    const ChassisPowerTelemetry_s *motor,
    const ChassisPowerControlTelemetry_s *control,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms,
    char *buffer,
    size_t buffer_size)
{
    const size_t control_length = RTTTelemetryFormatChassisPowerCsv(
        power, motor, control, tick_ms, measured_chassis_power_w,
        meter_online, meter_tick_ms, buffer, buffer_size);
    if (control_length == 0U)
        return 0U;

    /* Replace the control row's CRLF so both snapshots remain in one row. */
    const size_t prefix_length = control_length - 2U;
    const size_t remaining = buffer_size - prefix_length;
    const int written = snprintf(
        buffer + prefix_length, remaining,
        ",%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%lu\r\n",
        (long)RoundTelemetryValue(motor->current_command[0]),
        (long)RoundTelemetryValue(motor->current_command[1]),
        (long)RoundTelemetryValue(motor->current_command[2]),
        (long)RoundTelemetryValue(motor->current_command[3]),
        (long)RoundTelemetryValue(motor->actual_speed_rpm[0]),
        (long)RoundTelemetryValue(motor->actual_speed_rpm[1]),
        (long)RoundTelemetryValue(motor->actual_speed_rpm[2]),
        (long)RoundTelemetryValue(motor->actual_speed_rpm[3]),
        (unsigned long)meter_tick_ms);
    if (written < 0 || (size_t)written >= remaining)
    {
        buffer[0] = '\0';
        return 0U;
    }
    return prefix_length + (size_t)written;
}
#endif

void RTTTelemetryWriteChassisPowerHeader(void)
{
    (void)BSPLogWriteString(chassis_power_header);
}

void RTTTelemetryWriteChassisPowerRow(
    const ChassisPowerOutput_s *power,
    const ChassisPowerTelemetry_s *motor,
    const ChassisPowerControlTelemetry_s *control,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms)
{
#if CHASSIS_POWER_CALIBRATION_ENABLE
    const size_t length = FormatChassisPowerCalibrationCsv(
#else
    const size_t length = RTTTelemetryFormatChassisPowerCsv(
#endif
        power,
        motor,
        control,
        tick_ms,
        measured_chassis_power_w,
        meter_online,
        meter_tick_ms,
        chassis_power_row,
        sizeof(chassis_power_row));
    if (length > 0U)
        (void)BSPLogWrite(chassis_power_row, (unsigned)length);
}
