CC = g++
CFLAGS = -I. -c -w -Wall -Werror -g -ggdb
LDFLAGS = -lm
LDLIBS = -lcheck


# Guard against \r\n line endings only in Cygwin
OSTYPE := $(shell uname)
ifneq ($(OSTYPE),Darwin)
	OSTYPE := $(shell uname -o)
	ifeq ($(OSTYPE),Cygwin)
		TEST_SET_OPTS = igncr
	endif
endif

OBJS = $(wildcard *.c)

all: atcommander.o

clean:
	rm -rf *.o *.bin
