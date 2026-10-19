CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
ifneq ($(strip $(CHASSIS_POWER_CALIBRATION_ENABLE)),)
CFLAGS += -DCHASSIS_POWER_CALIBRATION_ENABLE=$(CHASSIS_POWER_CALIBRATION_ENABLE)
endif
TARGET := test_rtt_telemetry.exe
SOURCE := test_rtt_telemetry.c ../../modules/telemetry/rtt_telemetry.c
INCLUDES := -Istubs -I../../modules/telemetry -I../../modules/algorithm

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
