CC?=sh4-linux-gcc
CFLAGS=-lpthread -g -Wall -pedantic -Wextra -Wformat -Werror -I../linux/include

TARGETS= pti_test

all:$(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)
