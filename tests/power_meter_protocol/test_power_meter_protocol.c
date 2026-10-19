#include "power_meter_protocol.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int failures;

#define ASSERT_TRUE(value) \
    do { if (!(value)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); ++failures; } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        const float expected_ = (expected); \
        const float actual_ = (actual); \
        if (fabsf(expected_ - actual_) > (tolerance)) { \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n", \
                   __FILE__, __LINE__, (double)expected_, (double)actual_); \
            ++failures; \
        } \
    } while (0)

static void TestDecodesKnownPositiveFrame(void)
{
    const uint8_t frame[8] = {0x00U, 0x0CU, 0x5FU, 0xB4U,
                              0x0CU, 0xB2U, 0x1FU, 0x1BU};
    PowerMeterMeasurement_s value = {0};

    ASSERT_TRUE(PowerMeterProtocolDecodeStatus(frame, 8U, &value));
    ASSERT_NEAR(0.012f, value.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(24.5f, value.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(3.25f, value.current_a, 0.00001f);
    ASSERT_NEAR(79.63f, value.power_w, 0.00001f);
}

static void TestPreservesSignedValues(void)
{
    const uint8_t frame[8] = {0xFFU, 0xF4U, 0x12U, 0x34U,
                              0xFBU, 0x1EU, 0xF6U, 0x3CU};
    PowerMeterMeasurement_s value = {0};

    ASSERT_TRUE(PowerMeterProtocolDecodeStatus(frame, 8U, &value));
    ASSERT_NEAR(-0.012f, value.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(4.66f, value.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(-1.25f, value.current_a, 0.00001f);
    ASSERT_NEAR(-25.0f, value.power_w, 0.00001f);
}

static void TestDecodesSignedBoundaries(void)
{
    const uint8_t frame[8] = {0x7FU, 0xFFU, 0x80U, 0x00U,
                              0x7FU, 0xFFU, 0x80U, 0x00U};
    PowerMeterMeasurement_s value = {0};

    ASSERT_TRUE(PowerMeterProtocolDecodeStatus(frame, 8U, &value));
    ASSERT_NEAR(32.767f, value.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(-32.768f, value.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(32.767f, value.current_a, 0.00001f);
    ASSERT_NEAR(-327.68f, value.power_w, 0.0001f);
}

static void TestRejectsInvalidInputWithoutOverwritingOutput(void)
{
    const uint8_t frame[8] = {0};
    PowerMeterMeasurement_s value = {
        .shunt_voltage_v = 1.0f,
        .bus_voltage_v = 2.0f,
        .current_a = 3.0f,
        .power_w = 4.0f,
    };

    ASSERT_FALSE(PowerMeterProtocolDecodeStatus(frame, 7U, &value));
    ASSERT_NEAR(1.0f, value.shunt_voltage_v, 0.0f);
    ASSERT_NEAR(4.0f, value.power_w, 0.0f);
    ASSERT_FALSE(PowerMeterProtocolDecodeStatus(NULL, 8U, &value));
    ASSERT_FALSE(PowerMeterProtocolDecodeStatus(frame, 8U, NULL));
}

int main(void)
{
    TestDecodesKnownPositiveFrame();
    TestPreservesSignedValues();
    TestDecodesSignedBoundaries();
    TestRejectsInvalidInputWithoutOverwritingOutput();

    if (failures != 0)
        return 1;
    puts("PASS: power meter protocol tests");
    return 0;
}
