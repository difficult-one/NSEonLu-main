CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
SOURCE := test_chassis_spin.c ../../modules/algorithm/chassis_spin.c
SINE_SOURCE := test_chassis_spin_sine.c ../../modules/algorithm/chassis_spin.c \
               ../../modules/algorithm/chassis_power_transient.c ../../modules/algorithm/power_model.c
SINE_POWER_SOURCE := test_chassis_spin_sine_power.c ../../modules/algorithm/chassis_spin.c \
                     ../../modules/algorithm/chassis_power_transient.c ../../modules/algorithm/power_model.c \
                     ../../modules/algorithm/chassis_power_budget.c
BUFFER_SOURCE := test_chassis_spin_buffer.c ../../modules/algorithm/chassis_spin.c \
                 ../../modules/algorithm/chassis_power_budget.c
HEADERS := ../../modules/algorithm/chassis_spin.h ../../modules/algorithm/chassis_spin_config.h
INCLUDES := -I../../modules/algorithm
TARGETS := test_chassis_spin_fixed.exe test_chassis_spin_adaptive.exe test_chassis_spin_sine.exe \
           test_chassis_spin_fixed_override.exe test_chassis_spin_adaptive_override.exe \
           test_chassis_spin_sine_power.exe test_chassis_spin_buffer.exe

.PHONY: all test clean
all: $(TARGETS)

test_chassis_spin_fixed.exe: $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_ADAPTIVE_ENABLE=0 $(SOURCE) -lm -o $@

test_chassis_spin_adaptive.exe: $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_MODE=1 -DCHASSIS_SPIN_ADAPTIVE_ENABLE=1 $(SOURCE) -lm -o $@

test_chassis_spin_sine.exe: $(SINE_SOURCE) $(HEADERS) ../../modules/algorithm/chassis_power_transient.h ../../modules/algorithm/power_model.h
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_MODE=2 -DCHASSIS_SPIN_ADAPTIVE_ENABLE=0 $(SINE_SOURCE) -lm -o $@

test_chassis_spin_sine_power.exe: $(SINE_POWER_SOURCE) $(HEADERS) ../../modules/algorithm/chassis_power_transient.h ../../modules/algorithm/power_model.h ../../modules/algorithm/chassis_power_budget.h
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_MODE=2 $(SINE_POWER_SOURCE) -lm -o $@

test_chassis_spin_buffer.exe: $(BUFFER_SOURCE) $(HEADERS) ../../modules/algorithm/chassis_power_budget.h
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_MODE=2 $(BUFFER_SOURCE) -lm -o $@

test_chassis_spin_fixed_override.exe: $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_MODE=0 -DCHASSIS_SPIN_ADAPTIVE_ENABLE=1 $(SOURCE) -lm -o $@

test_chassis_spin_adaptive_override.exe: $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -DCHASSIS_SPIN_MODE=1 -DCHASSIS_SPIN_ADAPTIVE_ENABLE=0 $(SOURCE) -lm -o $@

test: $(TARGETS)
	./test_chassis_spin_fixed.exe
	./test_chassis_spin_adaptive.exe
	./test_chassis_spin_sine.exe
	./test_chassis_spin_fixed_override.exe
	./test_chassis_spin_adaptive_override.exe
	./test_chassis_spin_sine_power.exe
	./test_chassis_spin_buffer.exe

clean:
	-if exist test_chassis_spin_fixed.exe del /Q test_chassis_spin_fixed.exe
	-if exist test_chassis_spin_adaptive.exe del /Q test_chassis_spin_adaptive.exe
	-if exist test_chassis_spin_sine.exe del /Q test_chassis_spin_sine.exe
	-if exist test_chassis_spin_fixed_override.exe del /Q test_chassis_spin_fixed_override.exe
	-if exist test_chassis_spin_adaptive_override.exe del /Q test_chassis_spin_adaptive_override.exe
	-if exist test_chassis_spin_sine_power.exe del /Q test_chassis_spin_sine_power.exe
	-if exist test_chassis_spin_buffer.exe del /Q test_chassis_spin_buffer.exe
