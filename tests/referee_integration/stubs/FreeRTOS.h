#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H

void TestPlatformEnterCritical(void);
void TestPlatformExitCritical(void);

#define taskENTER_CRITICAL() TestPlatformEnterCritical()
#define taskEXIT_CRITICAL() TestPlatformExitCritical()

#endif
