CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_referee.exe
SOURCE := test_referee.c ../referee.c ../referee_crc.c
HEADERS := ../referee.h ../referee_protocol.h ../referee_crc.h
UI_TARGET := test_referee_ui.exe
UI_SOURCE := test_referee_ui.c ../referee_ui.c ../referee.c ../referee_crc.c
UI_HEADERS := ../referee_ui.h $(HEADERS)
INCLUDES := -I..

ifeq ($(OS),Windows_NT)
CLEAN := cmd /C "if exist $(TARGET) del /Q $(TARGET)"
UI_CLEAN := cmd /C "if exist $(UI_TARGET) del /Q $(UI_TARGET)"
else
CLEAN := $(RM) $(TARGET)
UI_CLEAN := $(RM) $(UI_TARGET)
endif

.PHONY: all test test-core test-ui clean
all: $(TARGET) $(UI_TARGET)
$(TARGET): $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)
$(UI_TARGET): $(UI_SOURCE) $(UI_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(UI_SOURCE) -o $(UI_TARGET)
test-core: $(TARGET)
	./$(TARGET)
test-ui: $(UI_TARGET)
	./$(UI_TARGET)
test: test-core test-ui
clean:
	$(CLEAN)
	$(UI_CLEAN)
