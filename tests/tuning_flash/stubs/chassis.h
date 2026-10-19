#include <stdbool.h>
#include <stdint.h>
typedef struct {bool valid; uint32_t tick_ms; uint8_t mode;} ChassisTelemetrySnapshot_s;
bool ChassisGetTelemetrySnapshot(ChassisTelemetrySnapshot_s *snapshot);
