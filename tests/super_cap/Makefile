CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_super_cap.exe
SOURCE := test_super_cap.c \
          ../../modules/super_cap/super_cap.c \
          ../../modules/super_cap/super_cap_protocol.c
INCLUDES := -Istubs -I../../modules/super_cap

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET) can_failure
	./$(TARGET) input_feedback
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
