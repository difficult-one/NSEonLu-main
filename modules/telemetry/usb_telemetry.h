#ifndef USB_TELEMETRY_H
#define USB_TELEMETRY_H

#include "usb_telemetry_config.h"
#include "usb_telemetry_format.h"

void USBTelemetryInit(void);
void USBTelemetryTask(void);
/* Single-producer stream: the caller polls once per 20 ms, never from an ISR. */
void USBTelemetryStreamInit(void);
void USBTelemetryStreamSend(USBTelemetrySample_s *sample, uint32_t now_ms);

#endif
