#ifndef REFEREE_POWER_H
#define REFEREE_POWER_H

#include <stdbool.h>
#include <stdint.h>

#define REFEREE_ROBOT_STATE_COMMAND_ID 0x0201U
#define REFEREE_POWER_HEAT_COMMAND_ID 0x0202U
#define REFEREE_ROBOT_STATE_PAYLOAD_LENGTH 13U
#define REFEREE_ROBOT_STATE_EXTENDED_PAYLOAD_LENGTH 17U
#define REFEREE_POWER_HEAT_PAYLOAD_LENGTH 14U
#define REFEREE_POWER_HEAT_LEGACY_PAYLOAD_LENGTH 16U
#define REFEREE_POWER_DATA_TIMEOUT_MS 300U

/* Enable only for a verified protocol that defines power at offset 4 (e.g. 2024).
 * The 2025 protocol also has 16-byte frames, but these bytes are reserved. */
#ifndef REFEREE_LEGACY_POWER_FIELDS_ENABLED
#define REFEREE_LEGACY_POWER_FIELDS_ENABLED 0
#endif

#if REFEREE_LEGACY_POWER_FIELDS_ENABLED != 0 && REFEREE_LEGACY_POWER_FIELDS_ENABLED != 1
#error "REFEREE_LEGACY_POWER_FIELDS_ENABLED must be 0 or 1"
#endif

typedef struct
{
    uint16_t chassis_power_limit_w;
    uint16_t buffer_energy_j;
    float chassis_power_w; /* Available only with explicitly enabled legacy power fields. */
    uint32_t robot_state_tick_ms;
    uint32_t power_heat_tick_ms;
    bool chassis_output_enabled;
    bool has_robot_state;
    bool has_power_heat;
    bool has_chassis_power; /* Latest frame provides finite power; check freshness separately. */
} RefereePowerState_s;

typedef struct
{
    bool valid;
    bool chassis_output_valid;
    bool chassis_output_enabled;
    uint16_t chassis_power_limit_w;
    uint16_t buffer_energy_j;
    uint32_t buffer_tick_ms;
    float chassis_power_w;
    bool chassis_power_valid;
} RefereePowerSnapshot_s;

void RefereePowerStateReset(RefereePowerState_s *state);
bool RefereePowerStateAcceptFrame(RefereePowerState_s *state,
                                  uint16_t command_id,
                                  const uint8_t *payload,
                                  uint16_t payload_length,
                                  uint32_t now_ms);
bool RefereePowerBuildSnapshot(const RefereePowerState_s *state,
                               uint32_t now_ms,
                               RefereePowerSnapshot_s *snapshot);

#endif
