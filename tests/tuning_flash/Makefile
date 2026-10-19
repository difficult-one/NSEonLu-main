CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
SOURCES = test_flash.c ../../modules/telemetry/tuning_flash.c ../../modules/telemetry/tuning_storage.c ../../modules/telemetry/runtime_tuning.c
test_flash.exe: $(SOURCES) $(wildcard stubs/*.h ../../modules/telemetry/tuning*.h ../../modules/telemetry/runtime_tuning.h)
	$(CC) $(CFLAGS) -Istubs -I../../modules/telemetry $(SOURCES) -o $@
test: test_flash.exe
	./test_flash.exe
