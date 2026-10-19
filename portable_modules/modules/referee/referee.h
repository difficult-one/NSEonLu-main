#ifndef PORTABLE_REFEREE_H
#define PORTABLE_REFEREE_H

#include "referee_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REFEREE_DEFAULT_OFFLINE_TIMEOUT_MS 300U

typedef bool (*RefereeUartSendFn)(void *context,
                                  const uint8_t *data,
                                  size_t length);
typedef uint32_t (*RefereeNowMsFn)(void *context);
typedef void (*RefereeCriticalFn)(void *context);

typedef struct
{
    RefereeUartSendFn uart_send;
    RefereeNowMsFn now_ms;
    RefereeCriticalFn enter_critical;
    RefereeCriticalFn exit_critical;
    void *context;
    uint32_t offline_timeout_ms;
} RefereeConfig;

typedef struct
{
    uint32_t valid_frame_count;
    uint32_t crc8_error_count;
    uint32_t crc16_error_count;
    uint32_t oversize_frame_count;
    uint32_t discarded_byte_count;
    uint32_t payload_length_error_count;
    uint32_t unsupported_command_count;
    uint32_t transmit_failure_count;
    uint32_t last_valid_frame_ms;
    bool has_valid_frame;
} RefereeDiagnostics;

typedef struct
{
    bool valid;
    bool chassis_output_enabled;
    uint16_t chassis_power_limit_w;
    uint16_t buffer_energy_j;
} RefereePower;

typedef struct Referee
{
    RefereeConfig config;
    uint8_t stream_buffer[REFEREE_STREAM_CAPACITY];
    size_t stream_used;
    RefereeData data;
    RefereeDiagnostics diagnostics;
    uint32_t robot_state_tick_ms;
    uint32_t power_heat_tick_ms;
    uint8_t tx_buffer[REFEREE_MAX_FRAME_SIZE];
    size_t tx_length;
    uint8_t tx_sequence;
    bool initialized;
} Referee;

bool RefereeInit(Referee *instance, const RefereeConfig *config);
bool RefereeFeed(Referee *instance, const uint8_t *data, size_t length);
bool RefereeGetData(Referee *instance, RefereeData *data);
bool RefereeGetPower(Referee *instance, RefereePower *power);
bool RefereeGetDiagnostics(Referee *instance,
                           RefereeDiagnostics *diagnostics);
bool RefereeIsOnline(Referee *instance);
uint16_t RefereeClientId(uint16_t robot_id);
bool RefereeSendInteraction(Referee *instance, uint16_t sender_id,
                            uint16_t receiver_id, uint16_t data_command_id,
                            const uint8_t *data, size_t data_length);

#endif
