#ifndef TEST_PLATFORM_H
#define TEST_PLATFORM_H

#include "bsp_usart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void TestPlatformReset(void);
void TestPlatformSetTick(uint32_t tick_ms);
void TestPlatformResetTickObservations(void);
bool TestPlatformFeed(const uint8_t *data, size_t length);
USARTInstance *TestPlatformGetUSART(void);
uint32_t TestPlatformTickCallsInsideCritical(void);
uint32_t TestPlatformTickCallsOutsideCritical(void);
uint32_t TestPlatformDaemonReloadCount(void);
int TestPlatformCriticalDepth(void);

#endif
