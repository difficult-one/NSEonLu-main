#include "super_cap.h"

#include <math.h>
#include <stddef.h>

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static void WriteU16LE(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static bool ReadFiniteF32LE(const uint8_t *bytes, float *value)
{
    const uint32_t bits = (uint32_t)bytes[0] |
                          ((uint32_t)bytes[1] << 8) |
                          ((uint32_t)bytes[2] << 16) |
                          ((uint32_t)bytes[3] << 24);
    const uint32_t exponent = (bits >> 23) & 0xFFU;
    const uint32_t fraction = bits & 0x7FFFFFU;
    float magnitude;

    if (value == NULL || exponent == 0xFFU)
        return false;

    if (exponent == 0U)
        magnitude = ldexpf((float)fraction, -149);
    else
        magnitude = ldexpf((float)(fraction | 0x800000U),
                           (int)exponent - 150);

    if ((bits & 0x80000000U) != 0U)
        magnitude = -magnitude;
    if (!isfinite(magnitude))
        return false;

    *value = magnitude;
    return true;
}

static void EnterCritical(SuperCap *instance)
{
    if (instance->config.enter_critical != NULL)
        instance->config.enter_critical(instance->config.context);
}

static void ExitCritical(SuperCap *instance)
{
    if (instance->config.exit_critical != NULL)
        instance->config.exit_critical(instance->config.context);
}

static bool RejectFrame(SuperCap *instance)
{
    EnterCritical(instance);
    ++instance->diagnostics.rejected_frame_count;
    ExitCritical(instance);
    return false;
}

bool SuperCapInit(SuperCap *instance, const SuperCapConfig *config)
{
    uint32_t command_id;
    uint32_t status_id;

    if (instance == NULL || config == NULL ||
        config->can_send == NULL || config->now_ms == NULL ||
        ((config->enter_critical == NULL) !=
         (config->exit_critical == NULL)))
    {
        return false;
    }

    command_id = config->command_id == 0U
                     ? SUPER_CAP_DEFAULT_COMMAND_ID
                     : config->command_id;
    status_id = config->status_id == 0U
                    ? SUPER_CAP_DEFAULT_STATUS_ID
                    : config->status_id;
    if (command_id > 0x7FFU || status_id > 0x7FFU)
        return false;

    *instance = (SuperCap){0};
    instance->config = *config;
    instance->config.command_id = command_id;
    instance->config.status_id = status_id;
    if (instance->config.offline_timeout_ms == 0U)
    {
        instance->config.offline_timeout_ms =
            SUPER_CAP_DEFAULT_OFFLINE_TIMEOUT_MS;
    }
    instance->initialized = true;
    return true;
}

bool SuperCapReceive(SuperCap *instance,
                     uint32_t standard_id,
                     const uint8_t *data,
                     uint8_t dlc)
{
    SuperCapStatus decoded;
    float chassis_power_w;
    uint32_t now_ms;

    if (instance == NULL || !instance->initialized)
        return false;
    if (standard_id != instance->config.status_id || data == NULL ||
        dlc != SUPER_CAP_FRAME_DLC)
    {
        return RejectFrame(instance);
    }

    if (!ReadFiniteF32LE(&data[1], &chassis_power_w))
        return RejectFrame(instance);

    decoded.online = true;
    decoded.output_enabled =
        (data[0] & SUPER_CAP_OUTPUT_DISABLED_MASK) == 0U;
    decoded.error_code = data[0] & SUPER_CAP_ERROR_MASK;
    decoded.chassis_power_w = chassis_power_w;
    decoded.available_power_limit_w = ReadU16LE(&data[5]);
    decoded.energy_ratio = (float)data[7] / 255.0f;
    now_ms = instance->config.now_ms(instance->config.context);

    EnterCritical(instance);
    instance->status = decoded;
    instance->has_status = true;
    ++instance->diagnostics.received_frame_count;
    instance->diagnostics.last_receive_tick_ms = now_ms;
    ExitCritical(instance);
    return true;
}

bool SuperCapSendCommand(SuperCap *instance,
                         const SuperCapCommand *command)
{
    uint8_t frame[SUPER_CAP_FRAME_DLC] = {0};
    bool sent;

    if (instance == NULL || !instance->initialized || command == NULL)
        return false;

    frame[0] = command->enable_dcdc ? 0x01U : 0x00U;
    WriteU16LE(&frame[1], command->referee_power_limit_w);
    WriteU16LE(&frame[3], command->referee_buffer_energy_j);

    sent = instance->config.can_send(instance->config.context,
                                     instance->config.command_id,
                                     frame,
                                     SUPER_CAP_FRAME_DLC);
    if (!sent)
    {
        EnterCritical(instance);
        ++instance->diagnostics.transmit_failure_count;
        ExitCritical(instance);
    }
    return sent;
}

bool SuperCapGetStatus(SuperCap *instance, SuperCapStatus *status)
{
    uint32_t last_receive_tick_ms;
    uint32_t timeout_ms;
    uint32_t now_ms;
    bool has_status;

    if (instance == NULL || !instance->initialized || status == NULL)
        return false;

    EnterCritical(instance);
    *status = instance->status;
    has_status = instance->has_status;
    last_receive_tick_ms = instance->diagnostics.last_receive_tick_ms;
    timeout_ms = instance->config.offline_timeout_ms;
    ExitCritical(instance);

    now_ms = instance->config.now_ms(instance->config.context);
    status->online = has_status &&
                     (uint32_t)(now_ms - last_receive_tick_ms) <= timeout_ms;
    return true;
}

bool SuperCapGetDiagnostics(SuperCap *instance,
                            SuperCapDiagnostics *diagnostics)
{
    if (instance == NULL || !instance->initialized || diagnostics == NULL)
        return false;

    EnterCritical(instance);
    *diagnostics = instance->diagnostics;
    ExitCritical(instance);
    return true;
}
