#include "chassis_kinematics.h"

#include <math.h>
#include <stdio.h>

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

#define ASSERT_NEAR(expected, actual, tolerance)                                        \
    do                                                                                  \
    {                                                                                   \
        const float expected_ = (expected);                                              \
        const float actual_ = (actual);                                                  \
        if (fabsf(expected_ - actual_) > (tolerance))                                   \
        {                                                                               \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n",                           \
                   __FILE__, __LINE__, (double)expected_, (double)actual_);              \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

static const float all_forward[CHASSIS_WHEEL_COUNT] = {
    1.0f, 1.0f, 1.0f, 1.0f,
};

static ChassisKinematicsInput_s Input(float vx, float vy, float wz)
{
    const ChassisKinematicsInput_s input = {
        .vx = vx,
        .vy = vy,
        .wz = wz,
        .rotation_lever_sum = {1.0f, 2.0f, 3.0f, 4.0f},
    };
    return input;
}

static void AssertOutput(const ChassisWheelOutput_s *output,
                         float left_front,
                         float right_front,
                         float left_back,
                         float right_back)
{
    ASSERT_NEAR(left_front, output->rpm[CHASSIS_WHEEL_LEFT_FRONT], 0.0001f);
    ASSERT_NEAR(right_front, output->rpm[CHASSIS_WHEEL_RIGHT_FRONT], 0.0001f);
    ASSERT_NEAR(left_back, output->rpm[CHASSIS_WHEEL_LEFT_BACK], 0.0001f);
    ASSERT_NEAR(right_back, output->rpm[CHASSIS_WHEEL_RIGHT_BACK], 0.0001f);
}

static void TestOmniXZeroInputStopsEveryWheel(void)
{
    const ChassisKinematicsInput_s input = Input(0.0f, 0.0f, 0.0f);
    ChassisWheelOutput_s output;

    ASSERT_TRUE(ChassisOmniXCalculate(&input, all_forward, &output));
    AssertOutput(&output, 0.0f, 0.0f, 0.0f, 0.0f);
}

static void TestOmniXForwardProjection(void)
{
    const ChassisKinematicsInput_s input = Input(10.0f, 0.0f, 0.0f);
    ChassisWheelOutput_s output;

    ASSERT_TRUE(ChassisOmniXCalculate(&input, all_forward, &output));
    AssertOutput(&output, -7.071068f, -7.071068f, 7.071068f, 7.071068f);
}

static void TestOmniXRightProjection(void)
{
    const ChassisKinematicsInput_s input = Input(0.0f, 10.0f, 0.0f);
    ChassisWheelOutput_s output;

    ASSERT_TRUE(ChassisOmniXCalculate(&input, all_forward, &output));
    AssertOutput(&output, -7.071068f, 7.071068f, -7.071068f, 7.071068f);
}

static void TestOmniXProjectsEachRotationalLever(void)
{
    const ChassisKinematicsInput_s input = Input(0.0f, 0.0f, 2.0f);
    ChassisWheelOutput_s output;

    ASSERT_TRUE(ChassisOmniXCalculate(&input, all_forward, &output));
    AssertOutput(&output, -1.414214f, -2.828427f, -4.242641f, -5.656854f);
}

static void TestOmniXDefaultGeometryDoesNotUseMecanumLeverDirectly(void)
{
    ChassisKinematicsInput_s input = Input(0.0f, 0.0f, 1.0f);
    ChassisWheelOutput_s output;
    for (int i = 0; i < CHASSIS_WHEEL_COUNT; ++i)
        input.rotation_lever_sum[i] = 325.0f;

    ASSERT_TRUE(ChassisOmniXCalculate(&input, all_forward, &output));
    AssertOutput(&output, -229.809708f, -229.809708f, -229.809708f, -229.809708f);
}

static void TestOmniXCombinesMotionAndWheelDirections(void)
{
    const ChassisKinematicsInput_s input = Input(2.0f, 4.0f, 3.0f);
    const float direction[CHASSIS_WHEEL_COUNT] = {1.0f, -1.0f, 1.0f, -1.0f};
    ChassisWheelOutput_s output;

    ASSERT_TRUE(ChassisOmniXCalculate(&input, direction, &output));
    AssertOutput(&output, -6.363961f, 2.828427f, -7.778175f, 4.242641f);
}

static void TestMecanumPreservesExistingFormula(void)
{
    const ChassisKinematicsInput_s input = Input(2.0f, 4.0f, 3.0f);
    ChassisWheelOutput_s output;

    ASSERT_TRUE(ChassisMecanumCalculate(&input, &output));
    AssertOutput(&output, -9.0f, -4.0f, -11.0f, -6.0f);
}

static void TestInvalidPointersAreRejected(void)
{
    const ChassisKinematicsInput_s input = Input(0.0f, 0.0f, 0.0f);
    ChassisWheelOutput_s output;

    ASSERT_FALSE(ChassisMecanumCalculate(NULL, &output));
    ASSERT_FALSE(ChassisMecanumCalculate(&input, NULL));
    ASSERT_FALSE(ChassisOmniXCalculate(NULL, all_forward, &output));
    ASSERT_FALSE(ChassisOmniXCalculate(&input, NULL, &output));
    ASSERT_FALSE(ChassisOmniXCalculate(&input, all_forward, NULL));
}

int main(void)
{
    TestOmniXZeroInputStopsEveryWheel();
    TestOmniXForwardProjection();
    TestOmniXRightProjection();
    TestOmniXProjectsEachRotationalLever();
    TestOmniXDefaultGeometryDoesNotUseMecanumLeverDirectly();
    TestOmniXCombinesMotionAndWheelDirections();
    TestMecanumPreservesExistingFormula();
    TestInvalidPointersAreRejected();

    if (failures != 0)
        return 1;

    puts("PASS: chassis kinematics tests");
    return 0;
}
