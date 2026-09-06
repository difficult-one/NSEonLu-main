#include "power_model.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define POWER_MODEL_EPSILON 1.0e-6f
#define POWER_MODEL_DISCRIMINANT_EPSILON 1.0e-5f

static float ClampFloat(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static void ResetOutput(ChassisPowerOutput_s *output, PowerModelStatus_e status)
{
    memset(output, 0, sizeof(*output));
    output->status = status;
}

bool PowerModelConfigIsValid(const MotorPowerModelConfig_s *config)
{
    return config != NULL && isfinite(config->k0) && isfinite(config->k1) &&
           isfinite(config->k2) && isfinite(config->k3) && isfinite(config->k4) &&
           isfinite(config->k5) && isfinite(config->current_conversion) &&
           config->current_conversion > 0.0f;
}

bool ChassisPowerAlgorithmConfigIsValid(const ChassisPowerAlgorithmConfig_s *config)
{
    if (config == NULL || !isfinite(config->safety_factor) ||
        !isfinite(config->small_error_threshold_rpm) ||
        !isfinite(config->reserved_power_threshold_w) ||
        !isfinite(config->per_motor_reserved_power_w) ||
        !isfinite(config->max_current_command))
        return false;

    return config->safety_factor > 0.0f && config->safety_factor <= 1.0f &&
           config->small_error_threshold_rpm >= 0.0f &&
           config->reserved_power_threshold_w >= 0.0f &&
           config->per_motor_reserved_power_w >= 0.0f &&
           config->reserved_power_threshold_w >=
               POWER_MODEL_MOTOR_COUNT * config->per_motor_reserved_power_w &&
           config->max_current_command > 0.0f;
}

float PowerModelPredict(const MotorPowerModelConfig_s *config,
                        float current_command,
                        float speed_rpm)
{
    if (!PowerModelConfigIsValid(config) || !isfinite(current_command) || !isfinite(speed_rpm))
        return NAN;

    const float current = fabsf(current_command / config->current_conversion);
    const float speed = fabsf(speed_rpm);
    return config->k0 + config->k1 * current + config->k2 * speed +
           config->k3 * current * speed + config->k4 * current * current +
           config->k5 * speed * speed;
}

static bool IsScaleInRange(float scale)
{
    return isfinite(scale) && scale >= 0.0f && scale <= 1.0f;
}

static float CalculateCurrentScale(const MotorPowerModelConfig_s *model,
                                   float desired_current,
                                   float speed_rpm,
                                   float power_limit_w)
{
    const float current = fabsf(desired_current / model->current_conversion);
    const float speed = fabsf(speed_rpm);
    const float a = model->k4 * current * current;
    const float b = (model->k1 + model->k3 * speed) * current;
    const float c = model->k0 + model->k2 * speed + model->k5 * speed * speed -
                    power_limit_w;

    if (fabsf(a) < POWER_MODEL_EPSILON)
    {
        if (fabsf(b) < POWER_MODEL_EPSILON)
            return c <= POWER_MODEL_EPSILON ? 1.0f : 0.0f;

        const float scale = -c / b;
        return IsScaleInRange(scale) ? scale : 0.0f;
    }

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < -POWER_MODEL_DISCRIMINANT_EPSILON)
        return 0.0f;
    if (discriminant < 0.0f)
        discriminant = 0.0f;

    const float root = sqrtf(discriminant);
    const float scale1 = (-b - root) / (2.0f * a);
    const float scale2 = (-b + root) / (2.0f * a);
    const bool scale1_valid = IsScaleInRange(scale1);
    const bool scale2_valid = IsScaleInRange(scale2);

    if (scale1_valid && scale2_valid)
        return scale1 > scale2 ? scale1 : scale2;
    if (scale1_valid)
        return scale1;
    if (scale2_valid)
        return scale2;
    return 0.0f;
}

static void AllocatePower(const ChassisPowerAlgorithmConfig_s *config,
                          const ChassisPowerInput_s *input,
                          ChassisPowerOutput_s *output)
{
    float total_error = 0.0f;
    float errors[POWER_MODEL_MOTOR_COUNT];

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        errors[i] = fabsf(input->speed_error_rpm[i]);
        total_error += errors[i];
    }

    if (total_error <= config->small_error_threshold_rpm)
    {
        const float share = output->effective_limit_w / POWER_MODEL_MOTOR_COUNT;
        for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
            output->allocated_power_w[i] = share;
        return;
    }

    if (output->effective_limit_w < config->reserved_power_threshold_w)
    {
        for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
            output->allocated_power_w[i] = errors[i] / total_error * output->effective_limit_w;
        return;
    }

    const float reserved_total = POWER_MODEL_MOTOR_COUNT * config->per_motor_reserved_power_w;
    const float proportional_power = output->effective_limit_w - reserved_total;
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        output->allocated_power_w[i] = config->per_motor_reserved_power_w +
                                       errors[i] / total_error * proportional_power;
    }
}

bool PowerModelApply(const MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT],
                     const ChassisPowerAlgorithmConfig_s *config,
                     const ChassisPowerInput_s *input,
                     ChassisPowerOutput_s *output)
{
    if (output == NULL)
        return false;
    if (models == NULL || config == NULL || input == NULL)
    {
        ResetOutput(output, POWER_MODEL_STATUS_INVALID_ARGUMENT);
        return false;
    }
    if (!ChassisPowerAlgorithmConfigIsValid(config))
    {
        ResetOutput(output, POWER_MODEL_STATUS_INVALID_CONFIG);
        return false;
    }

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        if (!PowerModelConfigIsValid(&models[i]))
        {
            ResetOutput(output, POWER_MODEL_STATUS_INVALID_CONFIG);
            return false;
        }
        if (!isfinite(input->desired_current[i]) || !isfinite(input->speed_rpm[i]) ||
            !isfinite(input->speed_error_rpm[i]))
        {
            ResetOutput(output, POWER_MODEL_STATUS_NUMERIC_ERROR);
            return false;
        }
    }

    if (!isfinite(input->referee_power_limit_w) || !isfinite(input->attenuation))
    {
        ResetOutput(output, POWER_MODEL_STATUS_NUMERIC_ERROR);
        return false;
    }

    ResetOutput(output, POWER_MODEL_STATUS_OK);
    output->effective_limit_w = input->referee_power_limit_w *
                                ClampFloat(input->attenuation, 0.0f, 1.0f) *
                                config->safety_factor;
    if (!isfinite(output->effective_limit_w) || output->effective_limit_w <= 0.0f)
    {
        ResetOutput(output, POWER_MODEL_STATUS_INVALID_BUDGET);
        return false;
    }

    AllocatePower(config, input, output);

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        const float desired_current = ClampFloat(input->desired_current[i],
                                                 -config->max_current_command,
                                                 config->max_current_command);
        output->predicted_unlimited_power_w[i] =
            PowerModelPredict(&models[i], desired_current, input->speed_rpm[i]);
        if (!isfinite(output->predicted_unlimited_power_w[i]))
        {
            ResetOutput(output, POWER_MODEL_STATUS_NUMERIC_ERROR);
            return false;
        }

        output->current_scale[i] = 1.0f;
        if (output->predicted_unlimited_power_w[i] > output->allocated_power_w[i])
        {
            output->current_scale[i] = CalculateCurrentScale(&models[i],
                                                             desired_current,
                                                             input->speed_rpm[i],
                                                             output->allocated_power_w[i]);
        }
        output->limited_current[i] = desired_current * output->current_scale[i];
        output->predicted_limited_power_w[i] =
            PowerModelPredict(&models[i], output->limited_current[i], input->speed_rpm[i]);
        if (!isfinite(output->limited_current[i]) ||
            !isfinite(output->predicted_limited_power_w[i]))
        {
            ResetOutput(output, POWER_MODEL_STATUS_NUMERIC_ERROR);
            return false;
        }
    }

    return true;
}
