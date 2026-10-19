CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
INCLUDES = -Istubs -I../../modules/telemetry -I../../Inc -I../../bsp/usb
FORMAT = ../../modules/telemetry/usb_telemetry_format.c
HEADERS = $(wildcard stubs/*.h ../../modules/telemetry/usb_telemetry*.h) ../../Inc/usbd_cdc_if.h ../../bsp/usb/bsp_usb.h
TRANSPORT = ../../modules/telemetry/usb_telemetry_stream.c ../../modules/telemetry/runtime_tuning.c ../../Src/usbd_cdc_if.c ../../bsp/usb/bsp_usb.c
TRANSPORT += ../../modules/telemetry/pid_autotune.c ../../modules/telemetry/autotune_protocol.c
REMOTE = ../../modules/remote/remote_control.c ../../modules/daemon/daemon.c
all: test_format.exe test_transport.exe test_remote.exe test_tuning.exe test_storage.exe

test_storage.exe: test_storage.c ../../modules/telemetry/tuning_storage.c ../../modules/telemetry/tuning_storage.h
	$(CC) $(CFLAGS) $(INCLUDES) test_storage.c ../../modules/telemetry/tuning_storage.c -o $@

test_tuning.exe: test_tuning.c ../../modules/telemetry/runtime_tuning.c ../../modules/telemetry/runtime_tuning.h
	$(CC) $(CFLAGS) $(INCLUDES) test_tuning.c ../../modules/telemetry/runtime_tuning.c -lm -o $@
test_format.exe: test_format.c $(FORMAT) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) test_format.c $(FORMAT) -lm -o $@
test_transport.exe: test_transport.c $(TRANSPORT) $(FORMAT) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) test_transport.c $(TRANSPORT) $(FORMAT) -lm -o $@
test_remote.exe: test_remote.c $(REMOTE) $(HEADERS) ../../modules/remote/remote_control.h ../../modules/daemon/daemon.h ../../bsp/usart/bsp_usart.h
	$(CC) $(CFLAGS) -Wno-unused-parameter $(INCLUDES) -I../../modules/remote -I../../modules/daemon -I../../bsp/usart test_remote.c $(REMOTE) -o $@
test: all
	./test_format.exe
	./test_transport.exe
	./test_remote.exe
	./test_tuning.exe
	./test_storage.exe
