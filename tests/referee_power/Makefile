CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_referee_power.exe
LEGACY_TARGET := test_referee_power_legacy.exe
SOURCE := test_referee_power.c ../../modules/referee/referee_power.c
HEADERS := ../../modules/referee/referee_power.h ../../modules/referee/referee_protocol.h
INCLUDES := -I../../modules/referee

.PHONY: all test clean

all: $(TARGET) $(LEGACY_TARGET)

$(TARGET): $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)

$(LEGACY_TARGET): $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) -DREFEREE_LEGACY_POWER_FIELDS_ENABLED=1 $(INCLUDES) $(SOURCE) -o $(LEGACY_TARGET)

test: $(TARGET) $(LEGACY_TARGET)
	./$(TARGET)
	./$(LEGACY_TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
	-if exist $(LEGACY_TARGET) del /Q $(LEGACY_TARGET)
