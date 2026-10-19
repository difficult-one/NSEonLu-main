#include "remote_control.h"
#include "bsp_usart.h"
#include "daemon.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static USARTInstance uart;
static uint32_t tick;
uint32_t HAL_GetTick(void) { return tick; }
USARTInstance *USARTRegister(USART_Init_Config_s *config)
{
    uart.module_callback = config->module_callback;
    return &uart;
}
void USARTServiceInit(USARTInstance *instance) { (void)instance; }

int main(void)
{
    RemoteControlSnapshot_s s;
    assert(!RemoteControlGetSnapshot(&s));
    UART_HandleTypeDef handle = {0};
    RemoteControlInit(&handle);
    assert(RemoteControlGetSnapshot(&s) && !s.has_sample && !s.online);
    /* Wire frame: centered channels except right X = -1, left switch UP, right MID. */
    const uint8_t frame[18] = {255,3,32,0,1,120,236,255,30,0,0,0,1,0,1,128,0,4};
    memcpy(uart.recv_buff, frame, sizeof frame);
    tick = 123;
    uart.module_callback();
    assert(RemoteControlGetSnapshot(&s));
    assert(s.has_sample && s.online && s.last_rx_tick_ms == 123);
    assert(s.control.rc.rocker_r_ == -1 && s.control.rc.rocker_l_ == 0);
    assert(s.control.rc.switch_left == 1 && s.control.rc.switch_right == 3);
    assert(s.control.mouse.x == -20 && s.control.mouse.y == 30);
    assert(s.control.key[KEY_PRESS].keys == 32769);
    s.control.rc.rocker_r_ = 99;
    assert(RemoteControlGetSnapshot(&s) && s.control.rc.rocker_r_ == -1);
    tick = 224;
    assert(RemoteControlGetSnapshot(&s) && !s.online); /* No daemon tick needed. */
    tick = 230;
    uart.module_callback();
    assert(RemoteControlGetSnapshot(&s) && s.online);
    for (unsigned i = 0; i < 11; ++i) DaemonTask();
    assert(RemoteControlGetSnapshot(&s) && !s.online && s.last_rx_tick_ms == 230);
    puts("USB telemetry remote snapshot tests passed");
    return 0;
}
