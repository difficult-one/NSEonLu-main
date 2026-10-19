CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
SRC = ../../modules/telemetry/pid_autotune.c
test: test_autotune.exe test_protocol.exe
	./test_autotune.exe
	./test_protocol.exe
test_protocol.exe: test_protocol.c $(SRC) ../../modules/telemetry/autotune_protocol.c ../../modules/telemetry/runtime_tuning.c
	$(CC) $(CFLAGS) -I../usb_telemetry/stubs -I../../modules/telemetry $^ -lm -o $@
test_autotune.exe: test_autotune.c $(SRC) ../../modules/telemetry/pid_autotune.h
	$(CC) $(CFLAGS) -I../../modules/telemetry test_autotune.c $(SRC) -lm -o $@
