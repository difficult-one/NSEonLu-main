#include "referee_power.h"
#include "referee_protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define ASSERT_TRUE(value)                                                     \
    do                                                                         \
    {                                                                          \
        if (!(value))                                                          \
        {                                                                      \
            ++failures;                                                        \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value);          \
        }                                                                      \
    } while (0)

#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))

#define ASSERT_EQ_U32(expected, actual)                                        \
    do                                                                         \
    {                                                                          \
        const uint32_t expected_value = (uint32_t)(expected);                  \
        const uint32_t actual_value = (uint32_t)(actual);                      \
        if (expected_value != actual_value)                                    \
        {                                                                      \
            ++failures;                                                        \
            fprintf(stderr,                                                    \
                    "FAIL line %d: expected %lu, got %lu\n",                  \
                    __LINE__,                                                   \
                    (unsigned long)expected_value,                             \
                    (unsigned long)actual_value);                              \
        }                                                                      \
    } while (0)

static const uint8_t robot_state_enabled[13] = {
    3U, 1U, 100U, 0U, 200U, 0U, 50U, 0U, 100U, 0U,
    120U, 0U, 0x02U};

static const uint8_t extended_robot_state[17] = {
    0x03U, 0x01U, 0xC8U, 0x00U, 0xC8U, 0x00U, 0x28U, 0x00U,
    0xF0U, 0x00U, 0x50U, 0x00U, 0x00U, 0x00U, 0xF0U, 0x41U, 0x07U};

static const uint8_t power_heat[14] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
    55U, 0U, 10U, 0U, 20U, 0U};

static const uint8_t legacy_power_heat[16] = {
    0xA8U, 0x61U, 0xF4U, 0x01U, 0U, 0U, 0x48U, 0x41U,
    55U, 0U, 10U, 0U, 20U, 0U, 30U, 0U};

static void TestAcceptsExtendedRobotStateUsingTrailingOutputFlags(void)
{
    RefereePowerState_s state;
    RefereePowerStateReset(&state);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, extended_robot_state, 17U, 250U));
    ASSERT_TRUE(state.has_robot_state);
    ASSERT_EQ_U32(80U, state.chassis_power_limit_w);
    ASSERT_TRUE(state.chassis_output_enabled);
    ASSERT_EQ_U32(250U, state.robot_state_tick_ms);
}

static void TestRobotStateOutputFlagsFollowPayloadLength(void)
{
    uint8_t extended_disabled[17];
    RefereePowerState_s state;
    RefereePowerStateReset(&state);
    memcpy(extended_disabled, extended_robot_state, sizeof(extended_disabled));
    extended_disabled[12] = 0x02U; /* Extension bytes do not grant output permission. */
    extended_disabled[16] = 0x05U; /* Gimbal and shooter on, chassis off. */

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));
    ASSERT_TRUE(state.chassis_output_enabled);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, extended_disabled, 17U, 150U));
    ASSERT_TRUE(state.has_robot_state);
    ASSERT_FALSE(state.chassis_output_enabled);
    ASSERT_EQ_U32(80U, state.chassis_power_limit_w);
    ASSERT_EQ_U32(150U, state.robot_state_tick_ms);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 200U));
    ASSERT_TRUE(state.chassis_output_enabled);
    ASSERT_EQ_U32(120U, state.chassis_power_limit_w);
    ASSERT_EQ_U32(200U, state.robot_state_tick_ms);
}

#if REFEREE_LEGACY_POWER_FIELDS_ENABLED
static void TestAcceptsLegacyPowerHeatFrame(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, legacy_power_heat, 16U, 150U));
    ASSERT_TRUE(state.has_power_heat);
    ASSERT_TRUE(state.has_chassis_power);
    ASSERT_TRUE(state.chassis_power_w == 12.5f);
    ASSERT_EQ_U32(150U, state.power_heat_tick_ms);
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 150U, &snapshot));
    ASSERT_EQ_U32(150U, snapshot.buffer_tick_ms);
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
    ASSERT_TRUE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 12.5f);
}
#else
static void Test16ByteReservedFieldsDoNotProvidePower(void)
{
    for (unsigned nonzero_reserved = 0U; nonzero_reserved < 2U; ++nonzero_reserved)
    {
        uint8_t payload[16];
        RefereePowerState_s state;
        RefereePowerSnapshot_s snapshot;
        memcpy(payload, legacy_power_heat, sizeof(payload));
        if (nonzero_reserved == 0U)
        {
            memset(payload, 0, 8U);
        }
        RefereePowerStateReset(&state);
        state.chassis_power_w = 99.0f;
        state.has_chassis_power = true;
        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0201U, robot_state_enabled, 13U, 100U));
        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0202U, payload, 16U, 150U));
        ASSERT_TRUE(state.has_power_heat);
        ASSERT_EQ_U32(150U, state.power_heat_tick_ms);
        ASSERT_EQ_U32(55U, state.buffer_energy_j);
        ASSERT_FALSE(state.has_chassis_power);
        ASSERT_TRUE(state.chassis_power_w == 0.0f);
        ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 150U, &snapshot));
        ASSERT_TRUE(snapshot.valid);
        ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
        ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
        ASSERT_FALSE(snapshot.chassis_power_valid);
        ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
    }
}
#endif

static void TestModernFrameClearsLegacyPowerAndIgnoresReservedBytes(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, legacy_power_heat, 16U, 100U));

    /* Same nonzero float bytes at offset 4 are reserved in a 14-byte frame. */
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, legacy_power_heat, 14U, 200U));
    ASSERT_FALSE(state.has_chassis_power);
    ASSERT_TRUE(state.chassis_power_w == 0.0f);
    ASSERT_EQ_U32(200U, state.power_heat_tick_ms);
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 200U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
    ASSERT_FALSE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
}

#if REFEREE_LEGACY_POWER_FIELDS_ENABLED
static void TestNonFiniteLegacyPowerIsUnavailableButBufferStillUpdates(void)
{
    static const uint8_t nonfinite_power[][4] = {
        {0U, 0U, 0xC0U, 0x7FU}, /* quiet NaN */
        {0U, 0U, 0x80U, 0x7FU}, /* +infinity */
        {0U, 0U, 0x80U, 0xFFU}, /* -infinity */
    };
    for (size_t i = 0U; i < sizeof(nonfinite_power) / sizeof(nonfinite_power[0]); ++i)
    {
        uint8_t payload[16];
        RefereePowerState_s state;
        RefereePowerSnapshot_s snapshot;
        RefereePowerStateReset(&state);
        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0201U, robot_state_enabled, 13U, 100U));
        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0202U, legacy_power_heat, 16U, 100U));
        memcpy(payload, legacy_power_heat, sizeof(payload));
        memcpy(&payload[4], nonfinite_power[i], 4U);
        payload[8] = 99U;

        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0202U, payload, 16U, 200U));
        ASSERT_FALSE(state.has_chassis_power);
        ASSERT_TRUE(state.chassis_power_w == 0.0f);
        ASSERT_EQ_U32(200U, state.power_heat_tick_ms);
        ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 200U, &snapshot));
        ASSERT_TRUE(snapshot.valid);
        ASSERT_EQ_U32(99U, snapshot.buffer_energy_j);
        ASSERT_FALSE(snapshot.chassis_power_valid);
        ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
    }
}

static void TestLegacyPowerExpiresWithEitherRequiredFrame(void)
{
    for (unsigned stale_frame = 0U; stale_frame < 2U; ++stale_frame)
    {
        RefereePowerState_s state;
        RefereePowerSnapshot_s snapshot;
        RefereePowerStateReset(&state);
        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0201U, robot_state_enabled, 13U, stale_frame == 0U ? 100U : 200U));
        ASSERT_TRUE(RefereePowerStateAcceptFrame(
            &state, 0x0202U, legacy_power_heat, 16U, stale_frame == 1U ? 100U : 200U));
        ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 400U, &snapshot));
        ASSERT_TRUE(snapshot.chassis_power_valid);
        ASSERT_TRUE(snapshot.chassis_power_w == 12.5f);
        ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 401U, &snapshot));
        ASSERT_FALSE(snapshot.valid);
        ASSERT_FALSE(snapshot.chassis_power_valid);
        ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
    }
}

static void TestWrongLengthsPreserveLegacyPowerAndTimestamp(void)
{
    uint8_t payload[17] = {0};
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, legacy_power_heat, 16U, 100U));
    memcpy(payload, legacy_power_heat, sizeof(legacy_power_heat));
    payload[6] = 0xC8U; /* 25 W */
    payload[8] = 99U;

    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, payload, 15U, 200U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, payload, 17U, 200U));
    ASSERT_EQ_U32(100U, state.power_heat_tick_ms);
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 200U, &snapshot));
    ASSERT_TRUE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 12.5f);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
}
#endif

static void TestRequiresBothFreshPowerFrames(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 1000U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 1000U, &snapshot));
    ASSERT_FALSE(snapshot.valid);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 1050U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 1050U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(snapshot.chassis_output_enabled);
    ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
}

static void TestRejectsWrongPayloadLengthsWithoutOverwritingState(void)
{
    static const uint8_t changed_robot_state[18] = {
        3U, 1U, 100U, 0U, 200U, 0U, 50U, 0U, 100U, 0U,
        231U, 3U, 0x00U};
    static const uint8_t changed_power_heat[14] = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
        99U, 0U, 10U, 0U, 20U, 0U};
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 10U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 10U));

    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, changed_robot_state, 12U, 20U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, changed_robot_state, 14U, 20U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, changed_robot_state, 16U, 20U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, changed_robot_state, 18U, 20U));
    ASSERT_EQ_U32(10U, state.robot_state_tick_ms);
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, changed_power_heat, 13U, 20U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, changed_power_heat, 15U, 20U));

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 20U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(snapshot.chassis_output_enabled);
    ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 311U, &snapshot));
    ASSERT_FALSE(snapshot.valid);
}

static void TestExpiresWhenEitherFrameIsOlderThan300Ms(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 200U));

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 400U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 401U, &snapshot));
    ASSERT_FALSE(snapshot.valid);
    ASSERT_FALSE(snapshot.chassis_output_enabled);
    ASSERT_EQ_U32(0U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(0U, snapshot.buffer_energy_j);
}

static void TestPowerHeatExpiresAfterExactly300Ms(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 100U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 400U));

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 400U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 401U, &snapshot));
    ASSERT_FALSE(snapshot.valid);
}

static void TestWrongRobotStateLengthDoesNotRefreshTimestamp(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));

    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 12U, 401U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 401U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 401U, &snapshot));
    ASSERT_FALSE(snapshot.valid);
}

static void TestWrongPowerHeatLengthDoesNotRefreshTimestamp(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 100U));

    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 13U, 401U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 401U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 401U, &snapshot));
    ASSERT_FALSE(snapshot.valid);
}

static void TestChassisOutputUsesBitOne(void)
{
    static const uint8_t robot_state_disabled[13] = {
        3U, 1U, 100U, 0U, 200U, 0U, 50U, 0U, 100U, 0U,
        120U, 0U, 0x05U};
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_disabled, 13U, 50U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 50U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 50U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_FALSE(snapshot.chassis_output_enabled);
}

static void TestFreshnessHandlesTickWrap(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, UINT32_MAX - 10U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, UINT32_MAX - 10U));

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 5U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
}

static void TestRejectsNullPointersAndUnknownCommands(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(NULL);
    RefereePowerStateReset(&state);

    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        NULL, 0x0201U, robot_state_enabled, 13U, 0U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, NULL, 13U, 0U));
    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x9999U, robot_state_enabled, 13U, 0U));
    ASSERT_FALSE(RefereePowerBuildSnapshot(NULL, 0U, &snapshot));
    ASSERT_FALSE(RefereePowerBuildSnapshot(&state, 0U, NULL));
}

int main(void)
{
    RefereePowerState_s partial_state = {0};
    RefereePowerSnapshot_s partial_snapshot = {0};
    partial_state.has_robot_state = true;
    partial_state.robot_state_tick_ms = 100;
    partial_state.chassis_output_enabled = false;
    ASSERT_TRUE(RefereePowerBuildSnapshot(&partial_state, 200, &partial_snapshot));
    ASSERT_FALSE(partial_snapshot.valid);
    ASSERT_TRUE(partial_snapshot.chassis_output_valid);
    ASSERT_FALSE(partial_snapshot.chassis_output_enabled);
    TestAcceptsExtendedRobotStateUsingTrailingOutputFlags();
    TestRobotStateOutputFlagsFollowPayloadLength();
#if REFEREE_LEGACY_POWER_FIELDS_ENABLED
    TestAcceptsLegacyPowerHeatFrame();
    TestNonFiniteLegacyPowerIsUnavailableButBufferStillUpdates();
    TestLegacyPowerExpiresWithEitherRequiredFrame();
    TestWrongLengthsPreserveLegacyPowerAndTimestamp();
#else
    Test16ByteReservedFieldsDoNotProvidePower();
#endif
    TestModernFrameClearsLegacyPowerAndIgnoresReservedBytes();
    TestRequiresBothFreshPowerFrames();
    TestRejectsWrongPayloadLengthsWithoutOverwritingState();
    TestExpiresWhenEitherFrameIsOlderThan300Ms();
    TestPowerHeatExpiresAfterExactly300Ms();
    TestWrongRobotStateLengthDoesNotRefreshTimestamp();
    TestWrongPowerHeatLengthDoesNotRefreshTimestamp();
    TestChassisOutputUsesBitOne();
    TestFreshnessHandlesTickWrap();
    TestRejectsNullPointersAndUnknownCommands();

    if (failures == 0)
    {
        printf("PASS: referee power tests\n");
        return 0;
    }

    fprintf(stderr, "FAIL: referee power tests (%d failures)\n", failures);
    return 1;
}
