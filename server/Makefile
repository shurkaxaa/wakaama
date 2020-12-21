SHELL = /bin/sh
CC    = gcc
FLAGS        = #-std=gnu99 -Iinclude
CFLAGS       = -I ../lib -I ../core -I ../examples/shared -fPIC -g -DLWM2M_SERVER_MODE -DLWM2M_LITTLE_ENDIAN
DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program
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
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../lib ./server
