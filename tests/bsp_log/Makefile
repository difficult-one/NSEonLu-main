CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_bsp_log.exe
SOURCE := test_bsp_log.c ../../bsp/log/bsp_log.c
INCLUDES := -Istubs -I../../Inc -I../../bsp/log

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -DUART1_USE=0 $(INCLUDES) $(SOURCE) -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
