#include "board_config.h"

#ifndef EXPECT_REFEREE
#error EXPECT_REFEREE is required
#endif
#ifndef EXPECT_UART_LOG
#error EXPECT_UART_LOG is required
#endif
#ifndef EXPECT_RTT
#error EXPECT_RTT is required
#endif

_Static_assert(UART1_REFEREE_ENABLED == EXPECT_REFEREE, "referee flag mismatch");
_Static_assert(UART1_LOG_ENABLED == EXPECT_UART_LOG, "UART log flag mismatch");
_Static_assert(UART1_RTT_ENABLED == EXPECT_RTT, "RTT flag mismatch");

int main(void) { return 0; }
