#ifndef AUTOTUNE_PROTOCOL_H
#define AUTOTUNE_PROTOCOL_H
#include "pid_autotune.h"
#include <stddef.h>
typedef struct {
    bool owned,running;
    uint32_t trial;
    PIDAutotuneTrial test;
    float target_rpm;
} AutotuneDrive;
/* USB task mailbox; motor task owns execution and the trial engine. */
void AutotuneSubmit(const char *line);
size_t AutotuneReply(char *buffer,size_t capacity);
void AutotuneReplySent(void);
void AutotuneCancel(void);
void AutotuneMotorStep(const PIDAutotuneFeedback *feedback,AutotuneDrive *drive);
void AutotuneObserve(uint32_t now,float rpm,float raw,float final,bool limited,bool transmitted);
#endif
