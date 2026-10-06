CC = gcc
CFLAGS = -g -Wall -Wextra -fsanitize=address,undefined -I../header_files

SRCS = $(wildcard ../src/*.c)
TEST_SRCS = test_db.c

test: $(SRCS) $(TEST_SRCS)
		$(CC) $(CFLAGS) $^ -o test

run: test
		./test

clean:
		rm -f test

.PHONY: test run clean
