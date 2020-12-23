SHELL = /bin/sh
CC    = gcc

ifneq ($(DEBUG),)
DEBUGFLAGS   = -O0 -D _DEBUG -DLWM2M_WITH_LOGS
endif
DEBUGFLAGS = -O3 -D NDEBUG -fwhole-program
FLAGS        = #-std=gnu99 -Iinclude
CFLAGS       = -I ../lib -I ../core -I ../examples/shared -fPIC -g -DLWM2M_SERVER_MODE -DLWM2M_LITTLE_ENDIAN $(DEBUGFLAGS)
LDFLAGS      = -lwakaama -L ../build

TARGET  = ../build/server
SOURCES = $(shell echo *.c)
HEADERS = $(shell echo *.h)
OBJECTS = $(SOURCES:.c=.o)

PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(OBJECTS)
	mkdir -p ../build
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

.PHONY : clean
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY : run
run:
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../build ../build/server
