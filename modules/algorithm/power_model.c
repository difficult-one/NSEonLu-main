#include "power_model.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static Chassis_Power_Control_Data_s *chassis_power_feedback;

static float ClampFloat(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static void ResetOutput(ChassisPowerOutput_s *output, PowerModelStatus_e status)
{
    memset(output, 0, sizeof(*output));
    output->status = status;
}

static void ResetFeedback(Chassis_Power_Control_Data_s *feedback)
{
    memset(feedback, 0, sizeof(*feedback));
}

bool PowerModelFindFeasibleScale(float a, float b, float c, float maximum, float *scale)
{
    if (scale == NULL) return false;
    *scale = 0;
    if (!isfinite(a) || !isfinite(b) || !isfinite(c) || !isfinite(maximum) || maximum < 0 || maximum > 1)
        return false;
    if ((a * maximum + b) * maximum + c <= 1e-5f) { *scale = maximum; return true; }
    if (fabsf(a) < 1e-6f) {
        if (b <= 1e-6f) return false;
        const float root = -c / b;
        if (root < 0 || root > maximum || !isfinite(root)) return false;
        *scale = root; return true;
    }
    const float discriminant = b * b - 4 * a * c;
    if (!isfinite(discriminant) || discriminant < 0) return false;
    const float q = -.5f * (b + copysignf(sqrtf(discriminant), b));
    float r0 = q / a, r1 = fabsf(q) > 1e-20f ? c / q : -b / (2 * a);
    if (r0 > r1) { const float swap = r0; r0 = r1; r1 = swap; }
    const float candidate = a > 0 ? fminf(maximum, r1) : fminf(maximum, r0);
    if (!isfinite(candidate) || candidate < 0 || (a > 0 && candidate < r0)) return false;
    if ((a * candidate + b) * candidate + c > 1e-4f) return false;
    *scale = candidate; return true;
}

bool PowerModelRecomputeFinal(const MotorPowerModelConfig_s models[4],
    const ChassisPowerInput_s *input, const float current[4], ChassisPowerOutput_s *output)
{
    if (models == NULL || input == NULL || current == NULL || output == NULL ||
        output->status != POWER_MODEL_STATUS_OK) return false;
    for (unsigned i = 0; i < 4; ++i) {
        output->limited_current[i] = current[i];
        output->current_scale[i] = fabsf(input->desired_current[i]) > 0 ?
            fabsf(current[i] / input->desired_current[i]) : 1;
        output->predicted_limited_power_w[i] = PowerModelPredict(&models[i], current[i], input->speed_rpm[i]);
        if (!isfinite(output->predicted_limited_power_w[i]) || !isfinite(current[i])) {
            ResetOutput(output, POWER_MODEL_STATUS_NUMERIC_ERROR); return false;
        }
    }
    return true;
}

bool ChassisPowerFeedbackInit(void)
{
    if (chassis_power_feedback != NULL)
        return false;

    chassis_power_feedback = malloc(sizeof(*chassis_power_feedback));
    if (chassis_power_feedback == NULL)
        return false;

    ResetFeedback(chassis_power_feedback);
    return true;
}

void ChassisPowerFeedbackUpdate(bool registered,
                                const ChassisPowerOutput_s *power)
{
    if (chassis_power_feedback == NULL)
        return;

    ResetFeedback(chassis_power_feedback);
    if (!registered || power == NULL || power->status != POWER_MODEL_STATUS_OK)
        return;

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        chassis_power_feedback->predicted_unlimited_power_w +=
            power->predicted_unlimited_power_w[i];
        chassis_power_feedback->predicted_limited_power_w +=
            power->predicted_limited_power_w[i];
    }
    chassis_power_feedback->effective_limit_w = power->effective_limit_w;

    if (!isfinite(chassis_power_feedback->predicted_unlimited_power_w) ||
        !isfinite(chassis_power_feedback->predicted_limited_power_w) ||
        !isfinite(chassis_power_feedback->effective_limit_w))
        ResetFeedback(chassis_power_feedback);
}

bool ChassisPowerFeedbackGet(Chassis_Power_Control_Data_s *feedback)
{
    if (feedback == NULL)
        return false;

    ResetFeedback(feedback);
    if (chassis_power_feedback == NULL)
        return false;

    *feedback = *chassis_power_feedback;
    return true;
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

float PowerModelSelectLimit(float referee_limit_w, float fallback_limit_w)
{
    if (isfinite(referee_limit_w) && referee_limit_w > 0.0f)
        return referee_limit_w;
    if (isfinite(fallback_limit_w) && fallback_limit_w > 0.0f)
        return fallback_limit_w;
    return referee_limit_w;
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

    float scale = 0;
    (void)PowerModelFindFeasibleScale(a, b, c, 1, &scale);
    return scale;
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

bool ChassisPowerTelemetryBuild(const ChassisPowerInput_s *input,
                                const ChassisPowerOutput_s *power,
                                const bool output_enabled[POWER_MODEL_MOTOR_COUNT],
                                ChassisPowerTelemetry_s *telemetry)
{
    if (telemetry == NULL)
        return false;

    memset(telemetry, 0, sizeof(*telemetry));
    if (input == NULL || power == NULL || output_enabled == NULL ||
        power->status != POWER_MODEL_STATUS_OK)
        return false;

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        const float target_speed_rpm = input->speed_rpm[i] + input->speed_error_rpm[i];
        if (!isfinite(power->limited_current[i]) || !isfinite(input->speed_rpm[i]) ||
            !isfinite(target_speed_rpm) || !isfinite(power->predicted_limited_power_w[i]))
        {
            memset(telemetry, 0, sizeof(*telemetry));
            return false;
        }

        telemetry->current_command[i] =
            output_enabled[i] ? power->limited_current[i] : 0.0f;
        telemetry->actual_speed_rpm[i] = input->speed_rpm[i];
        telemetry->target_speed_rpm[i] = target_speed_rpm;
        telemetry->predicted_power_w[i] =
            output_enabled[i] ? power->predicted_limited_power_w[i] : 0.0f;
        telemetry->predicted_total_power_w += telemetry->predicted_power_w[i];
        if (output_enabled[i])
            telemetry->motor_enabled_mask |= (uint8_t)(1U << i);
    }

    if (!isfinite(telemetry->predicted_total_power_w))
    {
        memset(telemetry, 0, sizeof(*telemetry));
        return false;
    }

    return true;
}
