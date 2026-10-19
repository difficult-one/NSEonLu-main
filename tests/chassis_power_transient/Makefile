CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
SOURCE := test_chassis_power_transient.c ../../modules/algorithm/chassis_power_transient.c ../../modules/algorithm/power_model.c
.PHONY: test
test:
	$(CC) $(CFLAGS) -I../../modules/algorithm $(SOURCE) -lm -o test_chassis_power_transient.exe
	./test_chassis_power_transient.exe
