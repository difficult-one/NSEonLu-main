#include "usb_telemetry_format.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static const char expected_header[] =
    "version,seq,tick,tx_drop_count,chassis_valid,chassis_tick_ms,chassis_mode,vx_cmd_milli,vy_cmd_milli,wz_cmd_milli,rc_online,rc_age_ms,rc_lx,rc_ly,rc_rx,rc_ry,rc_dial,rc_sw_left,rc_sw_right,mouse_x,mouse_y,mouse_left,mouse_right,keys,model_tick_ms,motor_online_mask,motor_enabled_mask,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf_rpm,target_rf_rpm,target_lb_rpm,target_rb_rpm,i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,i_lf_fb,i_rf_fb,i_lb_fb,i_rb_fb,power_valid,p_unlimited_cw,p_limited_cw,effective_limit_cw,p_meter_cw,meter_online,meter_age_ms,nominal_budget_cw,target_power_cw,power_source,referee_online,referee_limit_cw,buffer_j,buffer_age_ms,buffer_control_enabled,buffer_estimate_valid,buffer_lower_cj,buffer_predicted_cj,buffer_recovering,cap_online,cap_output_enabled,cap_error,cap_energy_pm,cap_chassis_cw,cap_limit_cw,cap_age_ms,cap_input_online,cap_input_cw,cap_input_age_ms\r\n";

static USBTelemetrySample_s fixture(void)
{
    USBTelemetrySample_s s = {0};
    s.version = 1; s.seq = 42; s.tick = 1000; s.tx_drop_count = 3;
    s.chassis_valid = 1; s.chassis_tick_ms = 995; s.chassis_mode = 2;
    s.vx_cmd_milli = -1250; s.vy_cmd_milli = 2500; s.wz_cmd_milli = -3000;
    s.rc_online = 1; s.rc_age_ms = 5; s.rc_lx = -660; s.rc_ly = 330;
    s.rc_rx = 660; s.rc_ry = -330; s.rc_dial = -100;
    s.rc_sw_left = 1; s.rc_sw_right = 3; s.mouse_x = -20; s.mouse_y = 30;
    s.mouse_left = 1; s.keys = 32769; s.model_tick_ms = 999;
    s.motor_online_mask = 15; s.motor_enabled_mask = 7;
    s.rpm_lf = -1234; s.rpm_rf = 1200; s.rpm_lb = -900; s.rpm_rb = 800;
    s.target_lf_rpm = -1500; s.target_rf_rpm = 1500;
    s.target_lb_rpm = -1000; s.target_rb_rpm = 1000;
    s.i_lf_cmd = -120; s.i_rf_cmd = 121; s.i_lb_cmd = -122; s.i_rb_cmd = 123;
    s.i_lf_fb = -110; s.i_rf_fb = 111; s.i_lb_fb = -112; s.i_rb_fb = 113;
    s.power_valid = 1; s.p_unlimited_cw = 2000; s.p_limited_cw = 1500;
    s.effective_limit_cw = 1800; s.p_meter_cw = 1234; s.meter_online = 1;
    s.meter_age_ms = 2; s.nominal_budget_cw = 8000; s.target_power_cw = 7840;
    s.power_source = 1; s.referee_online = 1; s.referee_limit_cw = 6000;
    s.buffer_j = 42; s.buffer_age_ms = 7; s.buffer_control_enabled = 1;
    s.buffer_estimate_valid = 1; s.buffer_lower_cj = 4123;
    s.buffer_predicted_cj = 3999; s.buffer_recovering = 1;
    s.cap_online = 1; s.cap_output_enabled = 1; s.cap_energy_pm = 750;
    s.cap_chassis_cw = 1278; s.cap_limit_cw = 12000; s.cap_age_ms = 3;
    s.cap_input_online = 1; s.cap_input_cw = 5678; s.cap_input_age_ms = 4;
    return s;
}

int main(int argc, char **argv)
{
    char header[2048], row[2048];
    USBTelemetrySample_s s = fixture();
    assert(USBTelemetryFormatHeader(header, sizeof header) == strlen(expected_header));
    assert(strcmp(header, expected_header) == 0);
    assert(USBTelemetryFormatRow(&s, row, sizeof row) > 0);
    if (argc > 1 && strcmp(argv[1], "--fixture") == 0) {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        fputs(header, stdout); fputs(row, stdout); return 0;
    }
    assert(strcmp(row,
        "1,42,1000,3,1,995,2,-1250,2500,-3000,1,5,-660,330,660,-330,-100,1,3,-20,30,1,0,32769,999,15,7,-1234,1200,-900,800,-1500,1500,-1000,1000,-120,121,-122,123,-110,111,-112,113,1,2000,1500,1800,1234,1,2,8000,7840,1,1,6000,42,7,1,1,4123,3999,1,1,1,0,750,1278,12000,3,1,5678,4\r\n") == 0);
    unsigned commas = 0;
    for (const char *p = row; *p; ++p) commas += *p == ',';
    assert(commas == 71);
    assert(USBTelemetryFixed(12.34f, 100) == 1234);
    assert(USBTelemetryFixed(-12.345f, 100) == -1235);
    assert(USBTelemetryFixed(1.25f, 1000) == 1250);
    assert(USBTelemetryFixed(INFINITY, 100) == 0);
    assert(USBTelemetryFixed(NAN, 100) == 0);
    assert(USBTelemetryFixed(1e30f, 100) == INT32_MAX);
    assert(USBTelemetryFixed(-1e30f, 100) == INT32_MIN);
    s.tick = UINT32_MAX; s.rc_age_ms = UINT32_MAX; s.vx_cmd_milli = INT32_MIN;
    assert(USBTelemetryFormatRow(&s, row, sizeof row));
    assert(strstr(row, "4294967295") != NULL);
    assert(strstr(row, "-2147483648") != NULL);
#define TEST_MAX_U(name) s.name = UINT32_MAX;
#define TEST_MAX_I(name) s.name = INT32_MIN;
    USB_TELEMETRY_FIELDS(TEST_MAX_U, TEST_MAX_I)
#undef TEST_MAX_U
#undef TEST_MAX_I
    assert(USBTelemetryFormatRow(&s, row, sizeof row) > 0);
    char short_row[8] = "before";
    assert(USBTelemetryFormatRow(&s, short_row, sizeof short_row) == 0);
    assert(short_row[0] == '\0');
    assert(USBTelemetryFormatHeader(short_row, sizeof short_row) == 0);
    assert(short_row[0] == '\0');
    assert(USBTelemetryFormatRow(NULL, row, sizeof row) == 0 && row[0] == '\0');
    assert(USBTelemetryFormatHeader(NULL, 0) == 0);
    s = fixture();
    USBTelemetryApplyFreshness(&s, 1000);
    assert(s.chassis_valid && s.power_valid && s.motor_online_mask == 15);
    s.rc_age_ms = 101; s.meter_age_ms = 21; s.buffer_age_ms = 301;
    s.cap_age_ms = 21; s.cap_input_age_ms = UINT32_MAX;
    USBTelemetryApplyFreshness(&s, 1200);
    assert(!s.chassis_valid && !s.power_valid && !s.motor_online_mask);
    assert(!s.motor_enabled_mask && !s.rc_online && !s.meter_online);
    assert(!s.referee_online && !s.buffer_estimate_valid && !s.cap_online);
    assert(!s.cap_output_enabled && !s.cap_input_online);
    s = fixture(); s.chassis_tick_ms = UINT32_MAX - 2; s.model_tick_ms = UINT32_MAX;
    USBTelemetryApplyFreshness(&s, 3);
    assert(s.chassis_valid && s.power_valid); /* Normal HAL tick wrap is fresh. */
    puts("USB telemetry format tests passed");
    return 0;
}
