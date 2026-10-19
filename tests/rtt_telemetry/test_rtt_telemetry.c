#include "rtt_telemetry.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
static unsigned captured_length;
static char captured_data[512];

#define ASSERT_TRUE(condition)                                                   \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            printf("FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition);         \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

#define ASSERT_STREQ(expected, actual) ASSERT_TRUE(strcmp((expected), (actual)) == 0)

unsigned BSPLogWrite(const void *data, unsigned length)
{
    if (length >= sizeof(captured_data))
        return 0U;
    memcpy(captured_data, data, length);
    captured_data[length] = '\0';
    captured_length = length;
    return length;
}

unsigned BSPLogWriteString(const char *text)
{
    return BSPLogWrite(text, (unsigned)strlen(text));
}

static void ResetCapture(void)
{
    captured_length = 0U;
    memset(captured_data, 0, sizeof(captured_data));
}

static ChassisPowerOutput_s ValidPowerOutput(void)
{
    return (ChassisPowerOutput_s){
        .current_scale = {1.0f, 1.0f, 1.0f, 1.0f},
        .status = POWER_MODEL_STATUS_OK,
    };
}

static void TestHeaderIsWrittenExactly(void)
{
    ResetCapture();
    RTTTelemetryWriteChassisPowerHeader();

    const char expected[] =
        "tick,nominal_budget_cw,target_power_cw,corrected_budget_cw,"
        "effective_limit_cw,p_unlimited_cw,p_limited_cw,p_meter_cw,"
        "p_filtered_cw,error_cw,correction_cw,min_scale_pm,limited_mask,"
        "outer_mode,meter_online,meter_age_ms,motor_enabled_mask"
#if CHASSIS_POWER_CALIBRATION_ENABLE
        ",i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,"
        "n_lf_rpm,n_rf_rpm,n_lb_rpm,n_rb_rpm,meter_tick_ms"
#endif
        "\r\n";
    ASSERT_STREQ(expected, captured_data);
    ASSERT_TRUE(captured_length == strlen(captured_data));
}

static void TestRepresentativeRowPreservesControlState(void)
{
    const ChassisPowerOutput_s power = {
        .effective_limit_w = 28.5f,
        .predicted_unlimited_power_w = {10.0f, 20.0f, 30.0f, 40.0f},
        .current_scale = {1.0f, 0.75f, 0.5f, 1.0f},
        .status = POWER_MODEL_STATUS_OK,
    };
    const ChassisPowerTelemetry_s motor = {
        .predicted_total_power_w = 25.25f,
        .motor_enabled_mask = 0x0FU,
    };
    const ChassisPowerControlTelemetry_s control = {
        .nominal_budget_w = 30.0f,
        .target_power_w = 28.5f,
        .corrected_budget_w = 30.0f,
        .filtered_power_w = 28.0f,
        .error_w = 0.5f,
        .correction_w = 0.0f,
        .outer_mode = 2U,
    };
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 123U, 29.0f, true, 120U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "123,3000,2850,3000,2850,10000,2525,2900,2800,50,0,"
        "500,6,2,1,3,15\r\n",
        row);
}

static void TestValidUnscaledOutputPrintsFullScale(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, 0.0f, false, 0U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "1,0,0,0,0,0,0,0,0,0,0,1000,0,0,0,0,0\r\n",
        row);
}

static void TestInvalidModelStateClearsModelDiagnostics(void)
{
    const ChassisPowerOutput_s power = {
        .effective_limit_w = 42.0f,
        .predicted_unlimited_power_w = {1.0f, 2.0f, 3.0f, 4.0f},
        .current_scale = {1.0f, 0.5f, 1.0f, 1.0f},
        .status = POWER_MODEL_STATUS_INVALID_BUDGET,
    };
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, 0.0f, false, 0U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\r\n",
        row);
}

static void TestMeterAgeSupportsNeverReceivedAndTickWrap(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 2U, 0.0f, true, 0U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "2,0,0,0,0,0,0,0,0,0,0,1000,0,0,1,0,0\r\n",
        row);

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 2U, 0.0f, true,
                    UINT32_MAX - 2U, row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "2,0,0,0,0,0,0,0,0,0,0,1000,0,0,1,5,0\r\n",
        row);
}

static void TestMeasuredPowerConversionHandlesSpecialValues(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, -1.25f, false, 0U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "1,0,0,0,0,0,0,-125,0,0,0,1000,0,0,0,0,0\r\n",
        row);

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, NAN, false, 0U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "1,0,0,0,0,0,0,0,0,0,0,1000,0,0,0,0,0\r\n",
        row);

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, FLT_MAX, false, 0U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "1,0,0,0,0,0,0,2147483647,0,0,0,1000,0,0,0,0,0\r\n",
        row);
}

static void TestInvalidFormattingInputsAreRejected(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};
    char row[8] = "dirty";

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    NULL, &motor, &control, 1U, 0.0f, false, 0U,
                    row, sizeof(row)) == 0U);
    ASSERT_TRUE(row[0] == '\0');

    strcpy(row, "dirty");
    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, 0.0f, false, 0U,
                    row, sizeof(row)) == 0U);
    ASSERT_TRUE(row[0] == '\0');

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &power, &motor, &control, 1U, 0.0f, false, 0U,
                    NULL, sizeof(row)) == 0U);
}

static void TestRowWriterSendsOneCompleteRow(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};

    ResetCapture();
    RTTTelemetryWriteChassisPowerRow(
        &power, &motor, &control, 7U, 1.5f, false, 0U);

    const char expected[] =
        "7,0,0,0,0,0,0,150,0,0,0,1000,0,0,0,0,0"
#if CHASSIS_POWER_CALIBRATION_ENABLE
        ",0,0,0,0,0,0,0,0,0"
#endif
        "\r\n";
    ASSERT_STREQ(expected, captured_data);
    ASSERT_TRUE(captured_length == strlen(captured_data));
}

static void TestWriterPreservesSignedCalibrationInputs(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {
        .current_command = {-123.6f, 200.4f, -300.5f, 400.5f},
        .actual_speed_rpm = {-100.6f, 200.4f, -300.5f, 400.5f},
        .predicted_total_power_w = 18.0f,
        .motor_enabled_mask = 15U,
    };
    const ChassisPowerControlTelemetry_s control = {0};

    ResetCapture();
    RTTTelemetryWriteChassisPowerRow(
        &power, &motor, &control, 123U, -1.25f, true, 117U);
    const char expected[] =
        "123,0,0,0,0,0,1800,-125,0,0,0,1000,0,0,1,6,15"
#if CHASSIS_POWER_CALIBRATION_ENABLE
        ",-124,200,-301,401,-101,200,-301,401,117"
#endif
        "\r\n";
    ASSERT_STREQ(expected, captured_data);
}

static void TestWriterNormalizesNonfiniteCalibrationValues(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {
        .current_command = {NAN, INFINITY, FLT_MAX, -FLT_MAX},
        .actual_speed_rpm = {-INFINITY, NAN, -FLT_MAX, FLT_MAX},
    };
    const ChassisPowerControlTelemetry_s control = {0};

    ResetCapture();
    RTTTelemetryWriteChassisPowerRow(
        &power, &motor, &control, 2U, 0.0f, false, UINT32_MAX - 2U);
    const char expected[] =
        "2,0,0,0,0,0,0,0,0,0,0,1000,0,0,0,5,0"
#if CHASSIS_POWER_CALIBRATION_ENABLE
        ",0,0,2147483647,-2147483648,0,0,-2147483648,2147483647,4294967293"
#endif
        "\r\n";
    ASSERT_STREQ(expected, captured_data);
    ASSERT_TRUE(captured_length < RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH);
}

static void TestInvalidWriterInputsProduceNoRow(void)
{
    const ChassisPowerOutput_s power = ValidPowerOutput();
    const ChassisPowerTelemetry_s motor = {0};
    const ChassisPowerControlTelemetry_s control = {0};
    ResetCapture();
    RTTTelemetryWriteChassisPowerRow(NULL, &motor, &control, 1U, 0, false, 0U);
    ASSERT_TRUE(captured_length == 0U);
    RTTTelemetryWriteChassisPowerRow(&power, NULL, &control, 1U, 0, false, 0U);
    ASSERT_TRUE(captured_length == 0U);
    RTTTelemetryWriteChassisPowerRow(&power, &motor, NULL, 1U, 0, false, 0U);
    ASSERT_TRUE(captured_length == 0U);
}

int main(void)
{
    TestHeaderIsWrittenExactly();
    TestRepresentativeRowPreservesControlState();
    TestValidUnscaledOutputPrintsFullScale();
    TestInvalidModelStateClearsModelDiagnostics();
    TestMeterAgeSupportsNeverReceivedAndTickWrap();
    TestMeasuredPowerConversionHandlesSpecialValues();
    TestInvalidFormattingInputsAreRejected();
    TestRowWriterSendsOneCompleteRow();
    TestWriterPreservesSignedCalibrationInputs();
    TestWriterNormalizesNonfiniteCalibrationValues();
    TestInvalidWriterInputsProduceNoRow();

    if (failures != 0)
        return 1;

    puts("PASS: RTT telemetry tests");
    return 0;
}
