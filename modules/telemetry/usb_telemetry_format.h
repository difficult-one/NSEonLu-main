#ifndef USB_TELEMETRY_FORMAT_H
#define USB_TELEMETRY_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/* CSV v1 wire order. Values are already fixed point at this boundary. */
#define USB_TELEMETRY_FIELDS(U, I) \
    U(version) U(seq) U(tick) U(tx_drop_count) \
    U(chassis_valid) U(chassis_tick_ms) U(chassis_mode) \
    I(vx_cmd_milli) I(vy_cmd_milli) I(wz_cmd_milli) \
    U(rc_online) U(rc_age_ms) I(rc_lx) I(rc_ly) I(rc_rx) I(rc_ry) I(rc_dial) \
    U(rc_sw_left) U(rc_sw_right) I(mouse_x) I(mouse_y) \
    U(mouse_left) U(mouse_right) U(keys) U(model_tick_ms) \
    U(motor_online_mask) U(motor_enabled_mask) \
    I(rpm_lf) I(rpm_rf) I(rpm_lb) I(rpm_rb) \
    I(target_lf_rpm) I(target_rf_rpm) I(target_lb_rpm) I(target_rb_rpm) \
    I(i_lf_cmd) I(i_rf_cmd) I(i_lb_cmd) I(i_rb_cmd) \
    I(i_lf_fb) I(i_rf_fb) I(i_lb_fb) I(i_rb_fb) \
    U(power_valid) I(p_unlimited_cw) I(p_limited_cw) I(effective_limit_cw) \
    I(p_meter_cw) U(meter_online) U(meter_age_ms) \
    I(nominal_budget_cw) I(target_power_cw) U(power_source) \
    U(referee_online) I(referee_limit_cw) U(buffer_j) U(buffer_age_ms) \
    U(buffer_control_enabled) U(buffer_estimate_valid) \
    I(buffer_lower_cj) I(buffer_predicted_cj) U(buffer_recovering) \
    U(cap_online) U(cap_output_enabled) U(cap_error) I(cap_energy_pm) \
    I(cap_chassis_cw) I(cap_limit_cw) U(cap_age_ms) \
    U(cap_input_online) I(cap_input_cw) U(cap_input_age_ms)

typedef struct {
#define USB_FIELD_U(name) uint32_t name;
#define USB_FIELD_I(name) int32_t name;
    USB_TELEMETRY_FIELDS(USB_FIELD_U, USB_FIELD_I)
#undef USB_FIELD_U
#undef USB_FIELD_I
} USBTelemetrySample_s;

/* Nonfinite inputs encode zero; the caller must clear the source validity flag. */
int32_t USBTelemetryFixed(float value, float scale);
/* Recheck source freshness at the instant a row is collected. */
void USBTelemetryApplyFreshness(USBTelemetrySample_s *sample, uint32_t now_ms);
/* Returns bytes excluding NUL, or zero with buffer[0] cleared on truncation. */
size_t USBTelemetryFormatHeader(char *buffer, size_t capacity);
size_t USBTelemetryFormatRow(const USBTelemetrySample_s *sample,
                            char *buffer, size_t capacity);

#endif
