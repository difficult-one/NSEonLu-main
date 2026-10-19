#include "chassis_power_link.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

static uint16_t BenchLimitToU16(float limit_w)
{
    if (limit_w >= (float)UINT16_MAX)
        return UINT16_MAX;
    return (uint16_t)(limit_w + 0.5f);
}

bool ChassisPowerLinkBuild(const ChassisPowerLinkInput_s *input,
                           ChassisPowerLinkOutput_s *output)
{
    if (input == NULL || output == NULL)
        return false;

    ChassisPowerLinkOutput_s result = {0};

    if (input->referee.valid)
    {
        result.command.enable_dcdc =
            input->referee.chassis_output_enabled &&
            input->referee.chassis_power_limit_w > 0U &&
            !input->chassis_zero_force;
        result.command.referee_power_limit_w =
            input->referee.chassis_power_limit_w;
        result.command.referee_buffer_energy_j =
            input->referee.buffer_energy_j;
        result.base_power_limit_w =
            (float)input->referee.chassis_power_limit_w;
    }
    else if (input->bench_enabled && isfinite(input->bench_limit_w) &&
             input->bench_limit_w > 0.0f)
    {
        result.command.enable_dcdc = !input->chassis_zero_force &&
            (!input->referee.chassis_output_valid || input->referee.chassis_output_enabled);
        result.command.referee_power_limit_w =
            BenchLimitToU16(input->bench_limit_w);
        result.base_power_limit_w = input->bench_limit_w;
    }

    *output = result;
    return true;
}
