CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_chassis_power_link.exe
SOURCE := test_chassis_power_link.c \
          ../../modules/super_cap/chassis_power_link.c \
          ../../modules/super_cap/super_cap_protocol.c
INCLUDES := -I../../modules/referee -I../../modules/super_cap

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
