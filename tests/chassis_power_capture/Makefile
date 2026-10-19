CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_chassis_power_capture.exe
SOURCE := test_chassis_power_capture.c ../../modules/telemetry/chassis_power_capture.c
INCLUDES := -I../../modules/telemetry
.PHONY: test
test:
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
	./$(TARGET)
