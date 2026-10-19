#include "usb_telemetry.h"
#include "tuning_flash.h"

#if USB_TELEMETRY_ENABLE
#include "usbd_cdc_if.h"
#include "chassis.h"
#include "remote_control.h"
#include "dji_motor.h"
#include "power_meter.h"
#include "rm_referee.h"
#include "super_cap.h"
#include "chassis_energy_config.h"
#include <limits.h>
#include <math.h>
#include <string.h>

/* Only the low-priority telemetry task owns this scratch storage. */
static USBTelemetrySample_s sample;
static DJIChassisPowerState_s model;
static ChassisTelemetrySnapshot_s chassis;
static RemoteControlSnapshot_s remote;

static uint32_t Age(uint32_t now_ms, uint32_t tick_ms, bool known)
{
    return known ? (uint32_t)(now_ms - tick_ms) : UINT32_MAX;
}

static void Collect(uint32_t now_ms)
{
    memset(&sample, 0, sizeof sample);
    sample.rc_age_ms = sample.meter_age_ms = sample.buffer_age_ms =
        sample.cap_age_ms = sample.cap_input_age_ms = UINT32_MAX;
    sample.power_source = CHASSIS_POWER_SOURCE_MODE;

    if (ChassisGetTelemetrySnapshot(&chassis)) {
        sample.chassis_valid = isfinite(chassis.vx_cmd) && isfinite(chassis.vy_cmd) &&
            isfinite(chassis.wz_cmd) && isfinite(chassis.nominal_budget_w) &&
            isfinite(chassis.target_power_w);
        sample.chassis_tick_ms = chassis.tick_ms;
        sample.chassis_mode = chassis.mode;
        sample.vx_cmd_milli = USBTelemetryFixed(chassis.vx_cmd, 1000);
        sample.vy_cmd_milli = USBTelemetryFixed(chassis.vy_cmd, 1000);
        sample.wz_cmd_milli = USBTelemetryFixed(chassis.wz_cmd, 1000);
        sample.nominal_budget_cw = USBTelemetryFixed(chassis.nominal_budget_w, 100);
        sample.target_power_cw = USBTelemetryFixed(chassis.target_power_w, 100);
        sample.buffer_control_enabled = chassis.buffer_control_enabled;
        sample.buffer_estimate_valid = chassis.buffer_estimate_valid &&
            isfinite(chassis.buffer_lower_j) && isfinite(chassis.buffer_predicted_j);
        sample.buffer_lower_cj = USBTelemetryFixed(chassis.buffer_lower_j, 100);
        sample.buffer_predicted_cj = USBTelemetryFixed(chassis.buffer_predicted_j, 100);
        sample.buffer_recovering = chassis.buffer_recovering;
    }
    if (RemoteControlGetSnapshot(&remote)) {
        sample.rc_online = remote.online;
        sample.rc_age_ms = Age(HAL_GetTick(), remote.last_rx_tick_ms, remote.has_sample);
        sample.rc_lx = remote.control.rc.rocker_l_;
        sample.rc_ly = remote.control.rc.rocker_l1;
        sample.rc_rx = remote.control.rc.rocker_r_;
        sample.rc_ry = remote.control.rc.rocker_r1;
        sample.rc_dial = remote.control.rc.dial;
        sample.rc_sw_left = remote.control.rc.switch_left;
        sample.rc_sw_right = remote.control.rc.switch_right;
        sample.mouse_x = remote.control.mouse.x;
        sample.mouse_y = remote.control.mouse.y;
        sample.mouse_left = remote.control.mouse.press_l != 0;
        sample.mouse_right = remote.control.mouse.press_r != 0;
        sample.keys = remote.control.key[KEY_PRESS].keys;
    }
    if (DJIChassisPowerGetSnapshot(&model) && model.registered) {
        now_ms = HAL_GetTick();
        sample.model_tick_ms = model.tick_ms;
        sample.motor_online_mask = model.online_mask & 15U;
        sample.motor_enabled_mask = model.telemetry.motor_enabled_mask & 15U;
        float unlimited_w = 0;
        bool finite_motors = true;
        for (unsigned i = 0; i < 4; ++i) {
            unlimited_w += model.power.predicted_unlimited_power_w[i];
            if ((uint32_t)(now_ms - model.feedback_tick_ms[i]) > 20U)
                sample.motor_online_mask &= ~(1U << i);
            if (!isfinite(model.telemetry.actual_speed_rpm[i]) ||
                !isfinite(model.telemetry.target_speed_rpm[i]) ||
                !isfinite(model.telemetry.current_command[i]) ||
                !isfinite(model.feedback_current[i])) {
                sample.motor_online_mask &= ~(1U << i);
                finite_motors = false;
            }
        }
        sample.rpm_lf = USBTelemetryFixed(model.telemetry.actual_speed_rpm[0], 1);
        sample.rpm_rf = USBTelemetryFixed(model.telemetry.actual_speed_rpm[1], 1);
        sample.rpm_lb = USBTelemetryFixed(model.telemetry.actual_speed_rpm[2], 1);
        sample.rpm_rb = USBTelemetryFixed(model.telemetry.actual_speed_rpm[3], 1);
        sample.target_lf_rpm = USBTelemetryFixed(model.telemetry.target_speed_rpm[0], 1);
        sample.target_rf_rpm = USBTelemetryFixed(model.telemetry.target_speed_rpm[1], 1);
        sample.target_lb_rpm = USBTelemetryFixed(model.telemetry.target_speed_rpm[2], 1);
        sample.target_rb_rpm = USBTelemetryFixed(model.telemetry.target_speed_rpm[3], 1);
        sample.i_lf_cmd = USBTelemetryFixed(model.telemetry.current_command[0], 1);
        sample.i_rf_cmd = USBTelemetryFixed(model.telemetry.current_command[1], 1);
        sample.i_lb_cmd = USBTelemetryFixed(model.telemetry.current_command[2], 1);
        sample.i_rb_cmd = USBTelemetryFixed(model.telemetry.current_command[3], 1);
        sample.i_lf_fb = USBTelemetryFixed(model.feedback_current[0], 1);
        sample.i_rf_fb = USBTelemetryFixed(model.feedback_current[1], 1);
        sample.i_lb_fb = USBTelemetryFixed(model.feedback_current[2], 1);
        sample.i_rb_fb = USBTelemetryFixed(model.feedback_current[3], 1);
        sample.power_valid = model.power.status == POWER_MODEL_STATUS_OK &&
            !model.numeric_error && finite_motors && isfinite(unlimited_w) &&
            isfinite(model.telemetry.predicted_total_power_w) &&
            isfinite(model.power.effective_limit_w);
        sample.p_unlimited_cw = USBTelemetryFixed(unlimited_w, 100);
        sample.p_limited_cw = USBTelemetryFixed(model.telemetry.predicted_total_power_w, 100);
        sample.effective_limit_cw = USBTelemetryFixed(model.power.effective_limit_w, 100);
    }
    PowerMeterStatus_s meter = {0};
    if (PowerMeterGetStatus(&meter)) {
        sample.meter_online = meter.online && isfinite(meter.power_w);
        sample.meter_age_ms = Age(HAL_GetTick(), meter.last_rx_tick_ms,
                                 meter.online || meter.last_rx_tick_ms != 0);
        sample.p_meter_cw = USBTelemetryFixed(meter.power_w, 100);
    }
    RefereePowerSnapshot_s referee = {0};
    if (RefereeGetPowerSnapshot(&referee)) {
        sample.referee_online = referee.valid;
        sample.referee_limit_cw = (int32_t)referee.chassis_power_limit_w * 100;
        sample.buffer_j = referee.buffer_energy_j;
        sample.buffer_age_ms = Age(HAL_GetTick(), referee.buffer_tick_ms,
                                  referee.valid || referee.buffer_tick_ms != 0);
    }
    SuperCapStatus_s cap = {0};
    if (SuperCapGetStatus(&cap)) {
        sample.cap_online = cap.online && isfinite(cap.energy_ratio) && isfinite(cap.chassis_power_w);
        sample.cap_output_enabled = cap.output_enabled;
        sample.cap_error = cap.error_code;
        sample.cap_energy_pm = USBTelemetryFixed(cap.energy_ratio, 1000);
        sample.cap_chassis_cw = USBTelemetryFixed(cap.chassis_power_w, 100);
        sample.cap_limit_cw = (int32_t)cap.available_power_limit_w * 100;
        sample.cap_age_ms = Age(HAL_GetTick(), cap.last_rx_tick_ms,
                               cap.online || cap.last_rx_tick_ms != 0);
    }
    SuperCapInputPower_s cap_input = {0};
    if (SuperCapGetInputPower(&cap_input)) {
        sample.cap_input_online = cap_input.online && isfinite(cap_input.input_power_w);
        sample.cap_input_cw = USBTelemetryFixed(cap_input.input_power_w, 100);
        sample.cap_input_age_ms = Age(HAL_GetTick(), cap_input.last_rx_tick_ms,
                                     cap_input.online || cap_input.last_rx_tick_ms != 0);
    }
    USBTelemetryApplyFreshness(&sample, HAL_GetTick());
}
#endif

void USBTelemetryInit(void)
{
#if USB_TELEMETRY_ENABLE
    USBTelemetryStreamInit();
#endif
}

void USBTelemetryTask(void)
{
#if USB_TELEMETRY_ENABLE
    /* Let the preceding USB reply finish before a potentially long sector erase. */
    if (!CDC_IsPortOpen_FS() || CDC_IsTransmitIdle_FS()) TuningFlashPoll();
    const uint32_t now_ms = HAL_GetTick();
    Collect(now_ms);
    USBTelemetryStreamSend(&sample, HAL_GetTick());
#endif
}
