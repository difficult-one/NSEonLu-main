#include "referee_power.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8U));
}

static float ReadF32LE(const uint8_t *bytes)
{
    const uint32_t bits = (uint32_t)bytes[0] |
                          ((uint32_t)bytes[1] << 8U) |
                          ((uint32_t)bytes[2] << 16U) |
                          ((uint32_t)bytes[3] << 24U);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void RefereePowerStateReset(RefereePowerState_s *state)
{
    if (state != NULL)
    {
        memset(state, 0, sizeof(*state));
    }
}

bool RefereePowerStateAcceptFrame(RefereePowerState_s *state,
                                  uint16_t command_id,
                                  const uint8_t *payload,
                                  uint16_t payload_length,
                                  uint32_t now_ms)
{
    if (state == NULL || payload == NULL)
    {
        return false;
    }

    if (command_id == REFEREE_ROBOT_STATE_COMMAND_ID)
    {
        if (payload_length != REFEREE_ROBOT_STATE_PAYLOAD_LENGTH &&
            payload_length != REFEREE_ROBOT_STATE_EXTENDED_PAYLOAD_LENGTH)
        {
            return false;
        }

        const uint16_t output_flags_offset =
            payload_length == REFEREE_ROBOT_STATE_EXTENDED_PAYLOAD_LENGTH
                ? 16U
                : 12U;
        state->chassis_power_limit_w = ReadU16LE(&payload[10]);
        state->chassis_output_enabled =
            (payload[output_flags_offset] & 0x02U) != 0U;
        state->robot_state_tick_ms = now_ms;
        state->has_robot_state = true;
        return true;
    }

    if (command_id == REFEREE_POWER_HEAT_COMMAND_ID)
    {
        if (payload_length != REFEREE_POWER_HEAT_PAYLOAD_LENGTH &&
            payload_length != REFEREE_POWER_HEAT_LEGACY_PAYLOAD_LENGTH)
        {
            return false;
        }

        const bool has_power_fields = REFEREE_LEGACY_POWER_FIELDS_ENABLED &&
            payload_length == REFEREE_POWER_HEAT_LEGACY_PAYLOAD_LENGTH;
        const float chassis_power_w = has_power_fields ? ReadF32LE(&payload[4]) : 0.0f;
        state->has_chassis_power = has_power_fields && isfinite(chassis_power_w);
        state->chassis_power_w = state->has_chassis_power ? chassis_power_w : 0.0f;
        state->buffer_energy_j = ReadU16LE(&payload[8]);
        state->power_heat_tick_ms = now_ms;
        state->has_power_heat = true;
        return true;
    }

    return false;
}

bool RefereePowerBuildSnapshot(const RefereePowerState_s *state,
                               uint32_t now_ms,
                               RefereePowerSnapshot_s *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    if (state == NULL)
    {
        return false;
    }

    snapshot->chassis_output_valid = state->has_robot_state &&
        (uint32_t)(now_ms - state->robot_state_tick_ms) <= REFEREE_POWER_DATA_TIMEOUT_MS;
    snapshot->chassis_output_enabled = snapshot->chassis_output_valid &&
        state->chassis_output_enabled;

    if (!state->has_robot_state || !state->has_power_heat ||
        (uint32_t)(now_ms - state->robot_state_tick_ms) >
            REFEREE_POWER_DATA_TIMEOUT_MS ||
        (uint32_t)(now_ms - state->power_heat_tick_ms) >
            REFEREE_POWER_DATA_TIMEOUT_MS)
    {
        return true;
    }

    snapshot->valid = true;
    snapshot->chassis_output_enabled = state->chassis_output_enabled;
    snapshot->chassis_power_limit_w = state->chassis_power_limit_w;
    snapshot->buffer_energy_j = state->buffer_energy_j;
    snapshot->buffer_tick_ms = state->power_heat_tick_ms;
    snapshot->chassis_power_valid = state->has_chassis_power;
    snapshot->chassis_power_w = state->chassis_power_w;
    return true;
}
