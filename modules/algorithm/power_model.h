#ifndef POWER_MODEL_H
#define POWER_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define POWER_MODEL_MOTOR_COUNT 4U

typedef enum
{
    POWER_MODEL_STATUS_OK = 0,
    POWER_MODEL_STATUS_INVALID_ARGUMENT,
    POWER_MODEL_STATUS_INVALID_CONFIG,
    POWER_MODEL_STATUS_INVALID_BUDGET,
    POWER_MODEL_STATUS_NUMERIC_ERROR,
} PowerModelStatus_e;

typedef struct
{
    float k0;
    float k1;
    float k2;
    float k3;
    float k4;
    float k5;
    float current_conversion;
} MotorPowerModelConfig_s;

typedef struct
{
    float safety_factor;
    float small_error_threshold_rpm;
    float reserved_power_threshold_w;
    float per_motor_reserved_power_w;
    float max_current_command;
} ChassisPowerAlgorithmConfig_s;

typedef struct
{
    float desired_current[POWER_MODEL_MOTOR_COUNT];
    float speed_rpm[POWER_MODEL_MOTOR_COUNT];
    float speed_error_rpm[POWER_MODEL_MOTOR_COUNT];
    float referee_power_limit_w;
    float attenuation;
} ChassisPowerInput_s;

typedef struct
{
    float effective_limit_w;
    float predicted_unlimited_power_w[POWER_MODEL_MOTOR_COUNT];
    float allocated_power_w[POWER_MODEL_MOTOR_COUNT];
    float current_scale[POWER_MODEL_MOTOR_COUNT];
    float limited_current[POWER_MODEL_MOTOR_COUNT];
    float predicted_limited_power_w[POWER_MODEL_MOTOR_COUNT];
    PowerModelStatus_e status;
} ChassisPowerOutput_s;

bool PowerModelConfigIsValid(const MotorPowerModelConfig_s *config);
bool ChassisPowerAlgorithmConfigIsValid(const ChassisPowerAlgorithmConfig_s *config);
float PowerModelPredict(const MotorPowerModelConfig_s *config,
                        float current_command,
                        float speed_rpm);
bool PowerModelApply(const MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT],
                     const ChassisPowerAlgorithmConfig_s *config,
                     const ChassisPowerInput_s *input,
                     ChassisPowerOutput_s *output);

#endif // POWER_MODEL_H
