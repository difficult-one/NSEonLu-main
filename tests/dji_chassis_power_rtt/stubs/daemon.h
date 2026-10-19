#ifndef TEST_DAEMON_H
#define TEST_DAEMON_H

#include <stdint.h>

typedef void (*offline_callback)(void *);

typedef struct
{
    uint8_t online;
} DaemonInstance;

typedef struct
{
    uint16_t reload_count;
    offline_callback callback;
    void *owner_id;
} Daemon_Init_Config_s;

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);

#endif
