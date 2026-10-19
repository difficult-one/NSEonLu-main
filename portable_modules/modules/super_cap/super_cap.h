#ifndef PORTABLE_SUPER_CAP_H
#define PORTABLE_SUPER_CAP_H

#include <stdbool.h>
#include <stdint.h>

#define SUPER_CAP_DEFAULT_COMMAND_ID 0x061U
#define SUPER_CAP_DEFAULT_STATUS_ID 0x051U
#define SUPER_CAP_FRAME_DLC 8U
#define SUPER_CAP_DEFAULT_OFFLINE_TIMEOUT_MS 200U

#define SUPER_CAP_ERROR_UNDER_VOLTAGE 0x01U
#define SUPER_CAP_ERROR_OVER_VOLTAGE 0x02U
#define SUPER_CAP_ERROR_BUCK_BOOST 0x04U
#define SUPER_CAP_ERROR_SHORT_CIRCUIT 0x08U
#define SUPER_CAP_ERROR_HIGH_TEMPERATURE 0x10U
#define SUPER_CAP_ERROR_NO_POWER_INPUT 0x20U
#define SUPER_CAP_ERROR_CAPACITOR 0x40U
#define SUPER_CAP_ERROR_MASK 0x7FU
#define SUPER_CAP_OUTPUT_DISABLED_MASK 0x80U

typedef bool (*SuperCapCanSendFn)(void *context,
                                  uint32_t standard_id,
                                  const uint8_t *data,
                                  uint8_t dlc);
typedef uint32_t (*SuperCapNowMsFn)(void *context);
typedef void (*SuperCapCriticalFn)(void *context);

typedef struct
{
    SuperCapCanSendFn can_send;
    SuperCapNowMsFn now_ms;
    SuperCapCriticalFn enter_critical;
    SuperCapCriticalFn exit_critical;
    void *context;
    uint32_t command_id;
    uint32_t status_id;
    uint32_t offline_timeout_ms;
} SuperCapConfig;

typedef struct
{
    bool enable_dcdc;
    uint16_t referee_power_limit_w;
    uint16_t referee_buffer_energy_j;
} SuperCapCommand;

typedef struct
{
    bool online;
    bool output_enabled;
    uint8_t error_code;
    float chassis_power_w;
    uint16_t available_power_limit_w;
    float energy_ratio;
} SuperCapStatus;

typedef struct
{
    uint32_t received_frame_count;
    uint32_t rejected_frame_count;
    uint32_t transmit_failure_count;
    uint32_t last_receive_tick_ms;
} SuperCapDiagnostics;

typedef struct
{
    SuperCapConfig config;
    SuperCapStatus status;
    SuperCapDiagnostics diagnostics;
    bool has_status;
    bool initialized;
} SuperCap;

bool SuperCapInit(SuperCap *instance, const SuperCapConfig *config);
bool SuperCapReceive(SuperCap *instance,
                     uint32_t standard_id,
                     const uint8_t *data,
                     uint8_t dlc);
bool SuperCapSendCommand(SuperCap *instance,
                         const SuperCapCommand *command);
bool SuperCapGetStatus(SuperCap *instance, SuperCapStatus *status);
bool SuperCapGetDiagnostics(SuperCap *instance,
                            SuperCapDiagnostics *diagnostics);

#endif
