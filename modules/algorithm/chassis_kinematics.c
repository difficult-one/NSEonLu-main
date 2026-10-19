#include "chassis_kinematics.h"

#include <stddef.h>

#define OMNI_X_PROJECTION 0.70710678118654752440f

bool ChassisMecanumCalculate(const ChassisKinematicsInput_s *input,
                             ChassisWheelOutput_s *output)
{
    if (input == NULL || output == NULL)
        return false;

    output->rpm[CHASSIS_WHEEL_LEFT_FRONT] =
        -input->vx - input->vy - input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_LEFT_FRONT];
    output->rpm[CHASSIS_WHEEL_RIGHT_FRONT] =
        -input->vx + input->vy - input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_RIGHT_FRONT];
    output->rpm[CHASSIS_WHEEL_LEFT_BACK] =
        input->vx - input->vy - input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_LEFT_BACK];
    output->rpm[CHASSIS_WHEEL_RIGHT_BACK] =
        input->vx + input->vy - input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_RIGHT_BACK];
    return true;
}

bool ChassisOmniXCalculate(const ChassisKinematicsInput_s *input,
                           const float wheel_direction[CHASSIS_WHEEL_COUNT],
                           ChassisWheelOutput_s *output)
{
    if (input == NULL || wheel_direction == NULL || output == NULL)
        return false;

    output->rpm[CHASSIS_WHEEL_LEFT_FRONT] =
        (-input->vx - input->vy -
         input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_LEFT_FRONT]) *
        OMNI_X_PROJECTION *
        wheel_direction[CHASSIS_WHEEL_LEFT_FRONT];
    output->rpm[CHASSIS_WHEEL_RIGHT_FRONT] =
        (-input->vx + input->vy -
         input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_RIGHT_FRONT]) *
        OMNI_X_PROJECTION *
        wheel_direction[CHASSIS_WHEEL_RIGHT_FRONT];
    output->rpm[CHASSIS_WHEEL_LEFT_BACK] =
        (input->vx - input->vy -
         input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_LEFT_BACK]) *
        OMNI_X_PROJECTION *
        wheel_direction[CHASSIS_WHEEL_LEFT_BACK];
    output->rpm[CHASSIS_WHEEL_RIGHT_BACK] =
        (input->vx + input->vy -
         input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_RIGHT_BACK]) *
        OMNI_X_PROJECTION *
        wheel_direction[CHASSIS_WHEEL_RIGHT_BACK];
    return true;
}
