#include "usb_telemetry_format.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define USB_HEADER_FIELD(name) #name ","
static const char header[] = USB_TELEMETRY_FIELDS(USB_HEADER_FIELD, USB_HEADER_FIELD);
#undef USB_HEADER_FIELD

int32_t USBTelemetryFixed(float value, float scale)
{
    if (!isfinite(value) || !isfinite(scale)) return 0;
    const double scaled = (double)value * (double)scale;
    if (scaled >= INT32_MAX) return INT32_MAX;
    if (scaled <= INT32_MIN) return INT32_MIN;
    return (int32_t)(scaled >= 0 ? scaled + 0.5 : scaled - 0.5);
}

void USBTelemetryApplyFreshness(USBTelemetrySample_s *sample, uint32_t now_ms)
{
    if (sample == NULL) return;
    if ((uint32_t)(now_ms - sample->chassis_tick_ms) > 100U) {
        sample->chassis_valid = 0;
        sample->buffer_estimate_valid = 0;
    }
    if ((uint32_t)(now_ms - sample->model_tick_ms) > 20U) {
        sample->power_valid = 0;
        sample->motor_online_mask = 0;
        sample->motor_enabled_mask = 0;
    }
    if (sample->rc_age_ms > 100U) sample->rc_online = 0;
    if (sample->meter_age_ms > 20U) sample->meter_online = 0;
    if (sample->buffer_age_ms > 300U) sample->referee_online = 0;
    if (!sample->referee_online) sample->buffer_estimate_valid = 0;
    if (sample->cap_age_ms > 20U) sample->cap_online = 0;
    if (!sample->cap_online) sample->cap_output_enabled = 0;
    if (sample->cap_input_age_ms > 20U) sample->cap_input_online = 0;
}

size_t USBTelemetryFormatHeader(char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) return 0;
    buffer[0] = '\0';
    /* Replace the last comma with CRLF and append NUL. */
    const size_t length = sizeof(header);
    if (capacity <= length) return 0;
    memcpy(buffer, header, length - 2);
    memcpy(buffer + length - 2, "\r\n", 3);
    return length;
}

size_t USBTelemetryFormatRow(const USBTelemetrySample_s *sample,
                            char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) return 0;
    buffer[0] = '\0';
    if (sample == NULL) return 0;
    size_t used = 0;
    int written;
#define USB_ROW_U(name) \
    written = snprintf(buffer + used, capacity - used, "%lu,", (unsigned long)sample->name); \
    if (written < 0 || (size_t)written >= capacity - used) goto truncated; \
    used += (size_t)written;
#define USB_ROW_I(name) \
    written = snprintf(buffer + used, capacity - used, "%ld,", (long)sample->name); \
    if (written < 0 || (size_t)written >= capacity - used) goto truncated; \
    used += (size_t)written;
    USB_TELEMETRY_FIELDS(USB_ROW_U, USB_ROW_I)
#undef USB_ROW_U
#undef USB_ROW_I
    if (capacity - used < 2) goto truncated;
    memcpy(buffer + used - 1, "\r\n", 3);
    return used + 1;
truncated:
    buffer[0] = '\0';
    return 0;
}
