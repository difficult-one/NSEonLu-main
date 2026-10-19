#ifndef TUNING_OVERRIDES_H
#define TUNING_OVERRIDES_H
/* Written by the local USB dashboard after a successful board acknowledgement.
 * Rebuild and flash to use these PC-side values as startup defaults.
 * Set a group's OVERRIDE to 0 to return to the original source defaults.
 * Field order is documented in runtime_tuning.h.
 * Flash-saved values take precedence; source overrides apply only to matching modes. */
#define TUNING_PID_OVERRIDE 1
#define TUNING_PID_MODE 0
#define TUNING_PID_VALUES {0.75f, 0.0f, 0.0f, 15000.0f, 3000.0f, 0.0f, 0.75f, 0.0f, 0.0f, 15000.0f, 3000.0f, 0.0f, 0.75f, 0.0f, 0.0f, 15000.0f, 3000.0f, 0.0f, 0.75f, 0.0f, 0.0f, 15000.0f, 3000.0f, 0.0f}
#define TUNING_POWER_OVERRIDE 0
#define TUNING_POWER_MODE 3
#define TUNING_POWER_VALUES {0}
#define TUNING_SPIN_OVERRIDE 0
#define TUNING_SPIN_MODE 2
#define TUNING_SPIN_VALUES {0}
#endif
