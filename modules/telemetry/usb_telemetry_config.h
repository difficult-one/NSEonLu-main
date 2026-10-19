#ifndef USB_TELEMETRY_CONFIG_H
#define USB_TELEMETRY_CONFIG_H

/* 1: CSV dashboard, 0: existing USB vision protocol. UART vision is independent. */
#ifndef USB_TELEMETRY_ENABLE
#define USB_TELEMETRY_ENABLE 1
#endif
#if USB_TELEMETRY_ENABLE != 0 && USB_TELEMETRY_ENABLE != 1
#error "USB_TELEMETRY_ENABLE must be 0 or 1"
#endif
#define USB_TELEMETRY_PERIOD_MS 20U
#define USB_TELEMETRY_HEADER_PERIOD_MS 1000U
#define USB_TELEMETRY_BUFFER_SIZE 2048U

#endif
