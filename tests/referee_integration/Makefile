CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_referee_integration.exe
LEGACY_TARGET := test_referee_integration_legacy.exe
SOURCE := test_referee_integration.c \
          stubs/test_platform.c \
          ../../modules/referee/rm_referee.c \
          ../../modules/referee/referee_stream_parser.c \
          ../../modules/referee/referee_power.c \
          ../../modules/super_cap/chassis_power_link.c \
          ../../modules/super_cap/super_cap_protocol.c \
          ../../modules/referee/crc_ref.c
INCLUDES := -Istubs -I../../modules/referee -I../../modules/super_cap
HEADERS := $(wildcard stubs/*.h ../../modules/referee/*.h ../../modules/super_cap/*.h)

.PHONY: all test clean

all: $(TARGET) $(LEGACY_TARGET)

$(TARGET): $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)

$(LEGACY_TARGET): $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) -DREFEREE_LEGACY_POWER_FIELDS_ENABLED=1 $(INCLUDES) $(SOURCE) -lm -o $(LEGACY_TARGET)

test: $(TARGET) $(LEGACY_TARGET)
	./$(TARGET)
	./$(LEGACY_TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
	-if exist $(LEGACY_TARGET) del /Q $(LEGACY_TARGET)
