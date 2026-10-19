#ifndef CHASSIS_KINEMATICS_H
#define CHASSIS_KINEMATICS_H

#include <stdbool.h>

typedef enum
{
    CHASSIS_WHEEL_LEFT_FRONT = 0,
    CHASSIS_WHEEL_RIGHT_FRONT,
    CHASSIS_WHEEL_LEFT_BACK,
    CHASSIS_WHEEL_RIGHT_BACK,
    CHASSIS_WHEEL_COUNT,
} ChassisWheelIndex_e;

typedef struct
{
    float vx;
    float vy;
    float wz;
    float rotation_lever_sum[CHASSIS_WHEEL_COUNT];
} ChassisKinematicsInput_s;

typedef struct
{
    float rpm[CHASSIS_WHEEL_COUNT];
} ChassisWheelOutput_s;

bool ChassisMecanumCalculate(const ChassisKinematicsInput_s *input,
                             ChassisWheelOutput_s *output);

bool ChassisOmniXCalculate(const ChassisKinematicsInput_s *input,
                           const float wheel_direction[CHASSIS_WHEEL_COUNT],
                           ChassisWheelOutput_s *output);

#endif // CHASSIS_KINEMATICS_H
