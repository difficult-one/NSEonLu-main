CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_chassis_power_overpower.exe
SOURCE := test_chassis_power_overpower.c \
          ../../modules/algorithm/chassis_power_outer_loop.c \
          ../../modules/algorithm/chassis_power_transient.c \
          ../../modules/algorithm/power_model.c
INCLUDES := -I../../modules/algorithm

.PHONY: all test
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
