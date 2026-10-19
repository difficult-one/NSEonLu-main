#ifndef CHASSIS_POWER_LINK_H
#define CHASSIS_POWER_LINK_H

#include "referee_power.h"
#include "super_cap_protocol.h"

#include <stdbool.h>

typedef struct
{
    RefereePowerSnapshot_s referee;
    bool bench_enabled;
    float bench_limit_w;
    bool chassis_zero_force;
} ChassisPowerLinkInput_s;

typedef struct
{
    SuperCapCommand_s command;
    float base_power_limit_w;
} ChassisPowerLinkOutput_s;

bool ChassisPowerLinkBuild(const ChassisPowerLinkInput_s *input,
                           ChassisPowerLinkOutput_s *output);

#endif
