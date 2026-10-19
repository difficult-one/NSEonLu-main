#include "runtime_tuning.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tuning_overrides.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t), "Tuning requires float32");
_Static_assert(TUNING_GROUP_COUNT == TUNING_STORED_GROUPS, "Update storage schema for group changes");
_Static_assert(TUNING_MAX_VALUES == TUNING_STORED_VALUES, "Update storage schema for field changes");
static const char *const names[] = {"pid", "power", "spin"};
static const unsigned counts[] = {24, 19, 16};
typedef struct {
    float values[TUNING_MAX_VALUES];
    uint32_t revision;
    unsigned mode;
    bool registered;
} GroupState;
static GroupState groups[TUNING_GROUP_COUNT];
static TuningStoredImage durable;
static unsigned dirty_mask;
static bool persistence_enabled, persistence_failed;
static bool trial_active;
void RuntimeTuningPersistenceInit(const TuningStoredImage *stored)
{
    durable=stored ? *stored : (TuningStoredImage){0};
    dirty_mask=0; persistence_failed=false; persistence_enabled=true;
}
static bool IsSaved(TuningGroup group,const GroupState *data)
{
    return (durable.mask&(1U<<group)) && durable.modes[group]==data->mode &&
        !memcmp(data->values,durable.values[group],counts[group]*sizeof(float));
}
static const char *PersistenceStatus(TuningGroup group,const GroupState *data)
{
    if (!persistence_enabled) return "unsupported";
    if (IsSaved(group,data)) return "saved";
    if (dirty_mask&(1U<<group)) return persistence_failed ? "error" : "pending";
    return "volatile";
}
bool RuntimeTuningPendingImage(TuningStoredImage *image)
{
    taskENTER_CRITICAL();
    if (!image || !persistence_enabled || !dirty_mask || persistence_failed || trial_active) {
        taskEXIT_CRITICAL(); return false;
    }
    *image=(TuningStoredImage){0};
    for (unsigned i=0;i<TUNING_GROUP_COUNT;++i) if (groups[i].registered) {
        image->mask |= 1U<<i; image->modes[i]=groups[i].mode;
        memcpy(image->values[i],groups[i].values,counts[i]*sizeof(float));
    }
    taskEXIT_CRITICAL();
    return true;
}
void RuntimeTuningPersistenceComplete(const TuningStoredImage *image,bool success)
{
    taskENTER_CRITICAL();
    if (success && image) {
        durable=*image;
        for (unsigned i=0;i<TUNING_GROUP_COUNT;++i)
            if (groups[i].registered && IsSaved((TuningGroup)i,&groups[i]))
                dirty_mask &= ~(1U<<i);
    }
    persistence_failed=!success;
    taskEXIT_CRITICAL();
}
static const float saved_values[TUNING_GROUP_COUNT][TUNING_MAX_VALUES] = {
    TUNING_PID_VALUES, TUNING_POWER_VALUES, TUNING_SPIN_VALUES};
static const bool saved_enabled[] = {
    TUNING_PID_OVERRIDE, TUNING_POWER_OVERRIDE, TUNING_SPIN_OVERRIDE};
static const unsigned saved_modes[] = {TUNING_PID_MODE,TUNING_POWER_MODE,TUNING_SPIN_MODE};
void RuntimeTuningRestore(TuningGroup group, bool (*apply)(const float *))
{
    if (group >= TUNING_GROUP_COUNT || !apply) return;
    if ((durable.mask&(1U<<group)) && durable.modes[group]==groups[group].mode &&
        apply(durable.values[group])) {
        memcpy(groups[group].values,durable.values[group],counts[group]*sizeof(float));
        return;
    }
    if (saved_enabled[group] && saved_modes[group]==groups[group].mode && apply(saved_values[group]))
        memcpy(groups[group].values, saved_values[group], counts[group] * sizeof(float));
}
enum { IDLE, QUEUED, APPLYING, READY };
static struct {
    unsigned state, generation;
    TuningGroup group;
    uint32_t id;
    char name[16];
    const char *error;
    GroupState data;
} mailbox;

unsigned RuntimeTuningCount(TuningGroup group)
{ return group < TUNING_GROUP_COUNT ? counts[group] : 0; }

void RuntimeTuningRegister(TuningGroup group, const float *values, unsigned mode)
{
    if (group >= TUNING_GROUP_COUNT || !values) return;
    taskENTER_CRITICAL();
    memcpy(groups[group].values, values, counts[group] * sizeof(float));
    groups[group].revision = 1;
    groups[group].mode = mode;
    groups[group].registered = true;
    taskEXIT_CRITICAL();
}
static bool ParseUnsigned(const char *text, uint32_t *value)
{
    if (!*text) return false;
    for (const char *p = text; *p; ++p) if (*p < '0' || *p > '9') return false;
    char *end;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (*end || parsed > UINT32_MAX) return false;
    *value = (uint32_t)parsed;
    return true;
}
static bool ParseFloat(const char *text, float *value)
{
    if (strlen(text) != 8) return false;
    uint32_t bits = 0;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned digit = (unsigned char)text[i];
        if (digit >= '0' && digit <= '9') digit -= '0';
        else if (digit >= 'a' && digit <= 'f') digit -= 'a' - 10;
        else if (digit >= 'A' && digit <= 'F') digit -= 'A' - 10;
        else return false;
        bits = (bits << 4) | digit;
    }
    memcpy(value, &bits, sizeof bits);
    return isfinite(*value);
}
void RuntimeTuningSubmit(const char *line)
{
    if (mailbox.state != IDLE || !line || strncmp(line, "@T1,", 4)) return;
    size_t length = strlen(line);
    if (length >= TUNING_LINE_SIZE) return;
    char copy[TUNING_LINE_SIZE];
    memcpy(copy, line, length + 1);
    char *cells[TUNING_MAX_VALUES + 6], *p = copy;
    unsigned n = 0;
    while (n < TUNING_MAX_VALUES + 6) {
        cells[n++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = 0; p = comma + 1;
    }
    uint32_t id;
    if (n < 4 || !ParseUnsigned(cells[1], &id) || strlen(cells[3]) >= sizeof mailbox.name)
        return;
    mailbox.id = id;
    strcpy(mailbox.name, cells[3]);
    mailbox.error = NULL;
    mailbox.state = READY;
    TuningGroup group;
    for (group = TUNING_PID; group < TUNING_GROUP_COUNT; ++group)
        if (!strcmp(cells[3], names[group])) break;
    mailbox.group = group;
    if (group == TUNING_GROUP_COUNT) {mailbox.error = "group"; return;}
    taskENTER_CRITICAL();
    mailbox.data = groups[group];
    taskEXIT_CRITICAL();
    if (!mailbox.data.registered) {mailbox.error = "unavailable"; return;}
    if (!strcmp(cells[2], "GET")) {
        if (n != 4) mailbox.error = "count";
        return;
    }
    if (trial_active) {mailbox.error="busy"; return;}
    bool saving=!strcmp(cells[2],"SAVE");
    if (strcmp(cells[2], "SET") && !saving) {mailbox.error = "operation"; return;}
    if (n != (saving ? 5 : counts[group] + 5)) {mailbox.error = "count"; return;}
    uint32_t revision;
    if (!ParseUnsigned(cells[4], &revision)) {mailbox.error = "version"; return;}
    if (revision != mailbox.data.revision) {mailbox.error = "conflict"; return;}
    if (saving) {
        if (!persistence_enabled) {mailbox.error="unsupported"; return;}
        taskENTER_CRITICAL();
        if (!IsSaved(group,&groups[group])) dirty_mask |= 1U<<group;
        persistence_failed=false;
        taskEXIT_CRITICAL();
        return;
    }
    for (unsigned i = 0; i < counts[group]; ++i)
        if (!ParseFloat(cells[i + 5], &mailbox.data.values[i])) {
            mailbox.error = "value"; return;
        }
    taskENTER_CRITICAL();
    mailbox.state = QUEUED;
    taskEXIT_CRITICAL();
}
void RuntimeTuningProcess(TuningGroup group, bool (*apply)(const float *))
{
    float values[TUNING_MAX_VALUES];
    unsigned generation;
    taskENTER_CRITICAL();
    if (mailbox.state != QUEUED || mailbox.group != group) {
        taskEXIT_CRITICAL(); return;
    }
    memcpy(values, mailbox.data.values, sizeof values);
    generation = mailbox.generation;
    mailbox.state = APPLYING;
    taskEXIT_CRITICAL();
    bool ok = apply && apply(values);
    taskENTER_CRITICAL();
    if (ok) {
        memcpy(groups[group].values, values, counts[group] * sizeof(float));
        ++groups[group].revision;
        if (persistence_enabled) {
            if (!IsSaved(group,&groups[group])) dirty_mask |= 1U<<group;
            else dirty_mask &= ~(1U<<group);
            persistence_failed=false;
        }
    }
    if (generation == mailbox.generation) {
        mailbox.data = groups[group];
        mailbox.error = ok ? NULL : "constraint";
        mailbox.state = READY;
    }
    taskEXIT_CRITICAL();
}
size_t RuntimeTuningReply(char *buffer, size_t capacity)
{
    if (mailbox.state != READY || !buffer || capacity < TUNING_LINE_SIZE) return 0;
    int length;
    if (mailbox.error)
        length = snprintf(buffer, capacity, "@T1,%lu,ERR,%s,%s\r\n",
            (unsigned long)mailbox.id, mailbox.name, mailbox.error);
    else {
        length = snprintf(buffer, capacity, "@T1,%lu,OK,%s,%lu,%u",
            (unsigned long)mailbox.id, mailbox.name,
            (unsigned long)mailbox.data.revision, mailbox.data.mode);
        for (unsigned i = 0; i < counts[mailbox.group]; ++i) {
            uint32_t bits; memcpy(&bits, &mailbox.data.values[i], sizeof bits);
            length += snprintf(buffer + length, capacity - (size_t)length,
                               ",%08lx", (unsigned long)bits);
        }
        taskENTER_CRITICAL();
        const char *status=PersistenceStatus(mailbox.group,&mailbox.data);
        taskEXIT_CRITICAL();
        length += snprintf(buffer + length, capacity - (size_t)length, ",%s\r\n",status);
    }
    return length > 0 && (size_t)length < capacity ? (size_t)length : 0;
}
void RuntimeTuningReplySent(void)
{
    taskENTER_CRITICAL();
    if (mailbox.state == READY) mailbox.state = IDLE;
    taskEXIT_CRITICAL();
}
void RuntimeTuningCancel(void)
{
    taskENTER_CRITICAL();
    ++mailbox.generation;
    mailbox.state = IDLE;
    taskEXIT_CRITICAL();
}
bool RuntimeTuningBeginTrial(uint32_t revision)
{
    taskENTER_CRITICAL();
    bool allowed=!trial_active && mailbox.state==IDLE && groups[TUNING_PID].registered &&
        groups[TUNING_PID].revision==revision;
    if (allowed) trial_active=true;
    taskEXIT_CRITICAL();
    return allowed;
}
void RuntimeTuningEndTrial(void)
{
    taskENTER_CRITICAL(); trial_active=false; taskEXIT_CRITICAL();
}
bool RuntimeTuningTrialActive(void)
{
    taskENTER_CRITICAL(); bool active=trial_active; taskEXIT_CRITICAL(); return active;
}
