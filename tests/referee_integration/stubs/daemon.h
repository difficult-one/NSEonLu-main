#ifndef TEST_DAEMON_H
#define TEST_DAEMON_H

#include <stdint.h>

typedef void (*offline_callback)(void *);

typedef struct daemon_ins
{
    uint16_t reload_count;
    offline_callback callback;
    uint16_t temp_count;
    void *owner_id;
} DaemonInstance;

typedef struct
{
    uint16_t reload_count;
    uint16_t init_count;
    offline_callback callback;
    void *owner_id;
} Daemon_Init_Config_s;

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config);
void DaemonReload(DaemonInstance *instance);

#endif
