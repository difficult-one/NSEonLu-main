CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -O0 -g \
          -ffunction-sections -fdata-sections
ifneq ($(strip $(CHASSIS_POWER_CALIBRATION_ENABLE)),)
CFLAGS += -DCHASSIS_POWER_CALIBRATION_ENABLE=$(CHASSIS_POWER_CALIBRATION_ENABLE)
endif
LDFLAGS := -Wl,--gc-sections
ifneq ($(strip $(CHASSIS_POWER_CONTROL_STAGE)),)
CFLAGS += -DCHASSIS_POWER_CONTROL_STAGE=$(CHASSIS_POWER_CONTROL_STAGE)
endif
ifneq ($(strip $(POWER_METER_ENABLE)),)
CFLAGS += -DPOWER_METER_ENABLE=$(POWER_METER_ENABLE)
endif
TARGET := test_dji_chassis_power_rtt.exe
SOURCE := test_dji_chassis_power_rtt.c \
          ../../modules/algorithm/power_model.c \
          ../../modules/algorithm/chassis_power_transient.c \
          ../../modules/telemetry/rtt_telemetry.c
SOURCE += ../../modules/telemetry/runtime_tuning.c
SOURCE += ../../modules/telemetry/pid_autotune.c ../../modules/telemetry/autotune_protocol.c
INCLUDES := -Istubs \
            -I../../modules/motor/DJImotor \
            -I../../modules/motor \
            -I../../modules/algorithm \
            -I../../modules/telemetry
INCLUDES += -I../../modules/power_meter

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE) ../../modules/motor/DJImotor/dji_motor.c ../../modules/motor/DJImotor/dji_motor.h
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) $(LDFLAGS) -lm -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
