CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_super_cap.exe
SOURCE := test_super_cap.c ../super_cap.c
INCLUDES := -I..
LDLIBS := -lm

ifeq ($(OS),Windows_NT)
CLEAN := powershell -NoProfile -Command "if (Test-Path -LiteralPath '$(TARGET)') { Remove-Item -LiteralPath '$(TARGET)' -Force }"
else
CLEAN := $(RM) $(TARGET)
endif

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET) $(LDLIBS)

test: $(TARGET)
	./$(TARGET)

clean:
	$(CLEAN)
