#include "chassis_power_link.h"
#include "crc_ref.h"
#include "rm_referee.h"
#include "super_cap_protocol.h"
#include "test_platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define ASSERT_TRUE(condition)                                                          \
    do                                                                                  \
    {                                                                                   \
        if (!(condition))                                                               \
        {                                                                               \
            printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ_U32(expected, actual)                                                  \
    do                                                                                  \
    {                                                                                   \
        const uint32_t expected_ = (uint32_t)(expected);                                \
        const uint32_t actual_ = (uint32_t)(actual);                                    \
        if (expected_ != actual_)                                                       \
        {                                                                               \
            printf("FAIL %s:%d: expected %lu, got %lu\n",                             \
                   __FILE__,                                                            \
                   __LINE__,                                                            \
                   (unsigned long)expected_,                                            \
                   (unsigned long)actual_);                                             \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

static const uint8_t extended_robot_payload[17] = {
    0x03U, 0x01U, 0xC8U, 0x00U, 0xC8U, 0x00U, 0x28U, 0x00U,
    0xF0U, 0x00U, 0x50U, 0x00U, 0x00U, 0x00U, 0xF0U, 0x41U, 0x07U,
};

static size_t BuildFrame(uint8_t *frame,
                         uint16_t command_id,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t sequence)
{
    const size_t frame_length = (size_t)payload_length + 9U;
    frame[0] = 0xA5U;
    frame[1] = (uint8_t)payload_length;
    frame[2] = (uint8_t)(payload_length >> 8U);
    frame[3] = sequence;
    frame[4] = 0U;
    Append_CRC8_Check_Sum(frame, 5U);
    frame[5] = (uint8_t)command_id;
    frame[6] = (uint8_t)(command_id >> 8U);
    if (payload_length > 0U)
        memcpy(&frame[7], payload, payload_length);
    frame[frame_length - 2U] = 0U;
    frame[frame_length - 1U] = 0U;
    Append_CRC16_Check_Sum(frame, (uint32_t)frame_length);
    return frame_length;
}

static referee_info_t *StartReferee(void)
{
    static UART_HandleTypeDef uart;
    TestPlatformReset();
    referee_info_t *info = RefereeInit(&uart);
    ASSERT_TRUE(info != NULL);
    ASSERT_TRUE(TestPlatformGetUSART()->module_callback != NULL);
    return info;
}

static void AssertFrameEquals(const uint8_t expected[8],
                              const uint8_t actual[8])
{
    for (size_t i = 0U; i < 8U; ++i)
        ASSERT_EQ_U32(expected[i], actual[i]);
}

static void TestSplitAndCoalescedFramesDriveCompleteCommand(void)
{
    static const uint8_t robot_state_payload[13] = {
        0x03U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x78U, 0x00U, 0x02U,
    };
    static const uint8_t power_heat_payload[14] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x37U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    };
    static const uint8_t expected_can_frame[8] = {
        0x01U, 0x78U, 0x00U, 0x37U, 0x00U, 0x00U, 0x00U, 0x00U,
    };
    uint8_t robot_frame[32];
    uint8_t power_frame[32];
    uint8_t second_chunk[64];
    const size_t robot_length = BuildFrame(
        robot_frame, 0x0201U, robot_state_payload, 13U, 0x21U);
    const size_t power_length = BuildFrame(
        power_frame, 0x0202U, power_heat_payload, 14U, 0x22U);

    StartReferee();
    TestPlatformSetTick(100U);
    ASSERT_TRUE(TestPlatformFeed(robot_frame, 8U));
    memcpy(second_chunk, &robot_frame[8], robot_length - 8U);
    memcpy(&second_chunk[robot_length - 8U], power_frame, power_length);
    ASSERT_TRUE(TestPlatformFeed(
        second_chunk, robot_length - 8U + power_length));

    TestPlatformResetTickObservations();
    RefereePowerSnapshot_s snapshot = {0};
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(snapshot.chassis_output_enabled);
    ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
    ASSERT_FALSE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
    ASSERT_EQ_U32(1U, TestPlatformTickCallsInsideCritical());
    ASSERT_EQ_U32(0U, TestPlatformTickCallsOutsideCritical());
    ASSERT_EQ_U32(0U, TestPlatformCriticalDepth());

    const ChassisPowerLinkInput_s input = {
        .referee = snapshot,
        .bench_enabled = false,
        .bench_limit_w = 0.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output = {0};
    uint8_t actual_can_frame[8] = {0};
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&output.command,
                                              actual_can_frame));
    AssertFrameEquals(expected_can_frame, actual_can_frame);
}

static void Test17ByteRobotStateMapsTrailingOutputFlags(void)
{
    uint8_t frame[32];
    const size_t length = BuildFrame(
        frame, 0x0201U, extended_robot_payload, 17U, 0x24U);

    referee_info_t *info = StartReferee();
    uint8_t saved_power_heat[sizeof(info->PowerHeatData)];
    memset(&info->PowerHeatData, 0xA5, sizeof(info->PowerHeatData));
    memcpy(saved_power_heat, &info->PowerHeatData, sizeof(saved_power_heat));
    TestPlatformSetTick(250U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));

    ASSERT_EQ_U32(3U, info->GameRobotState.robot_id);
    ASSERT_EQ_U32(1U, info->GameRobotState.robot_level);
    ASSERT_EQ_U32(200U, info->GameRobotState.current_HP);
    ASSERT_EQ_U32(200U, info->GameRobotState.maximum_HP);
    ASSERT_EQ_U32(40U, info->GameRobotState.shooter_barrel_cooling_value);
    ASSERT_EQ_U32(240U, info->GameRobotState.shooter_barrel_heat_limit);
    ASSERT_EQ_U32(80U, info->GameRobotState.chassis_power_limit);
    ASSERT_EQ_U32(1U, info->GameRobotState.power_management_gimbal_output);
    ASSERT_EQ_U32(1U, info->GameRobotState.power_management_chassis_output);
    ASSERT_EQ_U32(1U, info->GameRobotState.power_management_shooter_output);
    ASSERT_TRUE(memcmp(saved_power_heat, &info->PowerHeatData,
                       sizeof(saved_power_heat)) == 0);

    RefereeLinkDiagnostics_s diagnostics = {0};
    ASSERT_TRUE(RefereeGetLinkDiagnostics(&diagnostics));
    ASSERT_EQ_U32(0U, diagnostics.payload_length_error_count);

    RefereePowerSnapshot_s snapshot = {0};
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_FALSE(snapshot.valid);
    ASSERT_TRUE(snapshot.chassis_output_valid);
    ASSERT_TRUE(snapshot.chassis_output_enabled);
}

static void TestRobotStateOutputPermissionCanBeRevokedAcrossLayouts(void)
{
    static const uint8_t standard_robot_payload[13] = {
        0x04U, 0x01U, 0xC8U, 0x00U, 0xC8U, 0x00U, 0x28U, 0x00U,
        0xF0U, 0x00U, 0x5AU, 0x00U, 0x02U,
    };
    uint8_t disabled_payload[17];
    uint8_t frame[32];
    size_t length;
    RefereePowerSnapshot_s snapshot = {0};
    referee_info_t *info = StartReferee();

    TestPlatformSetTick(100U);
    length = BuildFrame(frame, 0x0201U, extended_robot_payload, 17U, 0x25U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.chassis_output_enabled);

    memcpy(disabled_payload, extended_robot_payload, sizeof(disabled_payload));
    disabled_payload[12] = 0x02U;
    disabled_payload[16] = 0x05U;
    TestPlatformSetTick(150U);
    length = BuildFrame(frame, 0x0201U, disabled_payload, 17U, 0x26U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    ASSERT_EQ_U32(1U, info->GameRobotState.power_management_gimbal_output);
    ASSERT_EQ_U32(0U, info->GameRobotState.power_management_chassis_output);
    ASSERT_EQ_U32(1U, info->GameRobotState.power_management_shooter_output);
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.chassis_output_valid);
    ASSERT_FALSE(snapshot.chassis_output_enabled);

    TestPlatformSetTick(200U);
    length = BuildFrame(frame, 0x0201U, standard_robot_payload, 13U, 0x27U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    ASSERT_EQ_U32(4U, info->GameRobotState.robot_id);
    ASSERT_EQ_U32(90U, info->GameRobotState.chassis_power_limit);
    ASSERT_EQ_U32(0U, info->GameRobotState.power_management_gimbal_output);
    ASSERT_EQ_U32(1U, info->GameRobotState.power_management_chassis_output);
    ASSERT_EQ_U32(0U, info->GameRobotState.power_management_shooter_output);
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.chassis_output_valid);
    ASSERT_TRUE(snapshot.chassis_output_enabled);

    RefereeLinkDiagnostics_s diagnostics = {0};
    ASSERT_TRUE(RefereeGetLinkDiagnostics(&diagnostics));
    ASSERT_EQ_U32(0U, diagnostics.payload_length_error_count);
    ASSERT_EQ_U32(3U, diagnostics.transport.valid_frame_count);
}

static void TestInvalidExtendedRobotLengthsPreserveStateAndFreshness(void)
{
    static const uint8_t standard_robot_payload[13] = {
        0x03U, 0x01U, 0xC8U, 0x00U, 0xC8U, 0x00U, 0x28U, 0x00U,
        0xF0U, 0x00U, 0x50U, 0x00U, 0x02U,
    };
    static const uint16_t invalid_lengths[] = {16U, 18U};
    uint8_t invalid_payload[18] = {0};
    uint8_t saved_robot_state[sizeof(ext_game_robot_state_t)];
    uint8_t frame[32];
    RefereePowerSnapshot_s snapshot = {0};
    referee_info_t *info = StartReferee();

    TestPlatformSetTick(100U);
    size_t length = BuildFrame(frame, 0x0201U, standard_robot_payload, 13U, 0x28U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    memcpy(saved_robot_state, &info->GameRobotState, sizeof(saved_robot_state));

    TestPlatformSetTick(400U);
    for (size_t i = 0U; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); ++i)
    {
        length = BuildFrame(frame, 0x0201U, invalid_payload, invalid_lengths[i], 0x29U);
        ASSERT_TRUE(TestPlatformFeed(frame, length));
        ASSERT_TRUE(memcmp(saved_robot_state, &info->GameRobotState,
                           sizeof(saved_robot_state)) == 0);
        ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
        ASSERT_TRUE(snapshot.chassis_output_valid);
        ASSERT_TRUE(snapshot.chassis_output_enabled);
    }

    TestPlatformSetTick(401U);
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_FALSE(snapshot.chassis_output_valid);
    ASSERT_FALSE(snapshot.chassis_output_enabled);
    RefereeLinkDiagnostics_s diagnostics = {0};
    ASSERT_TRUE(RefereeGetLinkDiagnostics(&diagnostics));
    ASSERT_EQ_U32(2U, diagnostics.payload_length_error_count);
    ASSERT_EQ_U32(3U, diagnostics.transport.valid_frame_count);
}

static void Test16BytePowerFrameUpdatesRawDataAndCommand(void)
{
    static const uint8_t robot_payload[13] = {
        0x03U, 0x01U, 0U, 0U, 0U, 0U, 0U,
        0U, 0U, 0U, 80U, 0U, 0x02U,
    };
    static const uint8_t legacy_payload[16] = {
        0xA8U, 0x61U, 0xF4U, 0x01U, 0U, 0U, 0x48U, 0x41U,
        55U, 0U, 10U, 0U, 20U, 0U, 30U, 0U,
    };
    static const uint8_t expected_can_frame[8] = {
        0x01U, 80U, 0U, 55U, 0U, 0U, 0U, 0U,
    };
    uint8_t robot_frame[32];
    uint8_t power_frame[32];
    const size_t robot_length = BuildFrame(
        robot_frame, 0x0201U, robot_payload, 13U, 0x25U);
    const size_t power_length = BuildFrame(
        power_frame, 0x0202U, legacy_payload, 16U, 0x26U);

    referee_info_t *info = StartReferee();
    info->GameRobotPos.x = 123.0f; /* Catch a 16-byte copy into the 14-byte structure. */
    TestPlatformSetTick(100U);
    ASSERT_TRUE(TestPlatformFeed(robot_frame, robot_length));
    ASSERT_TRUE(TestPlatformFeed(power_frame, 11U));
    ASSERT_TRUE(TestPlatformFeed(&power_frame[11], power_length - 11U));

    RefereePowerSnapshot_s snapshot = {0};
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ_U32(80U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
#if REFEREE_LEGACY_POWER_FIELDS_ENABLED
    ASSERT_TRUE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 12.5f);
#else
    ASSERT_FALSE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
#endif
    ASSERT_TRUE(info->PowerHeatData.reserved_power == 12.5f);
    ASSERT_EQ_U32(25000U, info->PowerHeatData.reserved_voltage);
    ASSERT_EQ_U32(500U, info->PowerHeatData.reserved_current);
    ASSERT_EQ_U32(10U, info->PowerHeatData.shooter_17mm_barrel_heat);
    ASSERT_EQ_U32(30U, info->PowerHeatData.shooter_42mm_barrel_heat);
    ASSERT_TRUE(info->GameRobotPos.x == 123.0f);

    RefereeLinkDiagnostics_s diagnostics = {0};
    ASSERT_TRUE(RefereeGetLinkDiagnostics(&diagnostics));
    ASSERT_EQ_U32(0U, diagnostics.payload_length_error_count);

    const ChassisPowerLinkInput_s input = {
        .referee = snapshot,
        .bench_enabled = false,
        .bench_limit_w = 0.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output = {0};
    uint8_t actual[8] = {0};
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&output.command, actual));
    AssertFrameEquals(expected_can_frame, actual);

    uint8_t changed_payload[16];
    memcpy(changed_payload, legacy_payload, sizeof(changed_payload));
    changed_payload[6] = 0xC8U; /* 25 W */
    changed_payload[8] = 99U;
    TestPlatformSetTick(300U);
    size_t changed_length = BuildFrame(
        power_frame, 0x0202U, changed_payload, 16U, 0x27U);
    power_frame[changed_length - 1U] ^= 1U;
    ASSERT_TRUE(TestPlatformFeed(power_frame, changed_length));
    changed_length = BuildFrame(
        power_frame, 0x0202U, changed_payload, 15U, 0x28U);
    ASSERT_TRUE(TestPlatformFeed(power_frame, changed_length));
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
#if REFEREE_LEGACY_POWER_FIELDS_ENABLED
    ASSERT_TRUE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 12.5f);
#else
    ASSERT_FALSE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
#endif
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
    ASSERT_TRUE(info->PowerHeatData.reserved_power == 12.5f);
    ASSERT_TRUE(RefereeGetLinkDiagnostics(&diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.transport.crc16_error_count);
    ASSERT_EQ_U32(1U, diagnostics.payload_length_error_count);

    /* A fresh robot frame cannot keep a rejected power frame alive. */
    TestPlatformSetTick(401U);
    ASSERT_TRUE(TestPlatformFeed(robot_frame, robot_length));
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_FALSE(snapshot.valid);
    ASSERT_FALSE(snapshot.chassis_power_valid);
    ASSERT_TRUE(snapshot.chassis_power_w == 0.0f);
}

static void TestOnlyOneRequiredFrameIsInvalid(void)
{
    static const uint8_t robot_state_payload[13] = {
        0x03U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x64U, 0x00U, 0x02U,
    };
    uint8_t frame[32];
    const size_t length = BuildFrame(
        frame, 0x0201U, robot_state_payload, 13U, 0x31U);

    StartReferee();
    TestPlatformSetTick(10U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));

    RefereePowerSnapshot_s snapshot = {0};
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_FALSE(snapshot.valid);
    ASSERT_EQ_U32(0U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(0U, snapshot.buffer_energy_j);
}

static void TestCrcAndPayloadLengthRejectionsAreDiagnosed(void)
{
    static const uint8_t robot_payload[13] = {
        0x03U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x50U, 0x00U, 0x02U,
    };
    static const uint8_t short_robot_payload[12] = {0};
    static const uint8_t short_power_payload[13] = {0};
    static const uint8_t short_game_payload[10] = {0};
    static const uint8_t unknown_payload[1] = {0x5AU};
    uint8_t frame[32];
    size_t length;

    StartReferee();
    TestPlatformSetTick(20U);

    length = BuildFrame(frame, 0x0201U, robot_payload, 13U, 0x40U);
    frame[length - 1U] ^= 0x01U;
    ASSERT_TRUE(TestPlatformFeed(frame, length));

    length = BuildFrame(frame, 0x0201U, short_robot_payload, 12U, 0x41U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    length = BuildFrame(frame, 0x0202U, short_power_payload, 13U, 0x42U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    length = BuildFrame(frame, 0x0001U, short_game_payload, 10U, 0x43U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));
    length = BuildFrame(frame, 0x7777U, unknown_payload, 1U, 0x44U);
    ASSERT_TRUE(TestPlatformFeed(frame, length));

    RefereePowerSnapshot_s snapshot = {0};
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_FALSE(snapshot.valid);

    RefereeLinkDiagnostics_s diagnostics = {0};
    ASSERT_TRUE(RefereeGetLinkDiagnostics(&diagnostics));
    ASSERT_EQ_U32(4U, diagnostics.transport.valid_frame_count);
    ASSERT_EQ_U32(1U, diagnostics.transport.crc16_error_count);
    ASSERT_EQ_U32(3U, diagnostics.payload_length_error_count);
    ASSERT_EQ_U32(4U, TestPlatformDaemonReloadCount());
}

static void TestFreshnessBoundaryFailsSafeAndRecovers(void)
{
    static const uint8_t robot_state_payload[13] = {
        0x03U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x5AU, 0x00U, 0x02U,
    };
    static const uint8_t power_heat_payload[14] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x19U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    };
    static const uint8_t expected_zero[8] = {0};
    static const uint8_t expected_recovered[8] = {
        0x01U, 0x5AU, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U, 0x00U,
    };
    uint8_t robot_frame[32];
    uint8_t power_frame[32];
    const size_t robot_length = BuildFrame(
        robot_frame, 0x0201U, robot_state_payload, 13U, 0x50U);
    const size_t power_length = BuildFrame(
        power_frame, 0x0202U, power_heat_payload, 14U, 0x51U);

    StartReferee();
    TestPlatformSetTick(100U);
    ASSERT_TRUE(TestPlatformFeed(robot_frame, robot_length));
    ASSERT_TRUE(TestPlatformFeed(power_frame, power_length));

    RefereePowerSnapshot_s snapshot = {0};
    TestPlatformSetTick(400U);
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.valid);

    TestPlatformSetTick(401U);
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_FALSE(snapshot.valid);

    ChassisPowerLinkInput_s input = {
        .referee = snapshot,
        .bench_enabled = false,
        .bench_limit_w = 0.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output = {0};
    uint8_t actual[8] = {0};
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&output.command, actual));
    AssertFrameEquals(expected_zero, actual);

    TestPlatformSetTick(500U);
    ASSERT_TRUE(TestPlatformFeed(robot_frame, robot_length));
    ASSERT_TRUE(TestPlatformFeed(power_frame, power_length));
    ASSERT_TRUE(RefereeGetPowerSnapshot(&snapshot));
    ASSERT_TRUE(snapshot.valid);
    input.referee = snapshot;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&output.command, actual));
    AssertFrameEquals(expected_recovered, actual);
}

static void TestNullOutputsAreRejected(void)
{
    StartReferee();
    ASSERT_FALSE(RefereeGetPowerSnapshot(NULL));
    ASSERT_FALSE(RefereeGetLinkDiagnostics(NULL));
}

int main(void)
{
    TestSplitAndCoalescedFramesDriveCompleteCommand();
    Test17ByteRobotStateMapsTrailingOutputFlags();
    TestRobotStateOutputPermissionCanBeRevokedAcrossLayouts();
    TestInvalidExtendedRobotLengthsPreserveStateAndFreshness();
    Test16BytePowerFrameUpdatesRawDataAndCommand();
    TestOnlyOneRequiredFrameIsInvalid();
    TestCrcAndPayloadLengthRejectionsAreDiagnosed();
    TestFreshnessBoundaryFailsSafeAndRecovers();
    TestNullOutputsAreRejected();

    if (failures != 0)
        return 1;

    puts("PASS: referee integration tests");
    return 0;
}
