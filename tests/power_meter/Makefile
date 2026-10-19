CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_power_meter.exe
SOURCE := test_power_meter.c \
          ../../modules/power_meter/power_meter.c \
          ../../modules/power_meter/power_meter_protocol.c
INCLUDES := -Istubs -I../../modules/power_meter

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET) can_failure
	./$(TARGET) custom_timeout
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
