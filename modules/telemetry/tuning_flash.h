#ifndef TUNING_FLASH_H
#define TUNING_FLASH_H
void TuningFlashInit(void);
/* Telemetry task only; saves pending values only with fresh stopped controls. */
void TuningFlashPoll(void);
#endif
