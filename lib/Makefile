SHELL = /bin/sh
CC    = gcc

ifneq ($(DEBUG),)
DEBUGFLAGS   = -O0 -D _DEBUG -DLWM2M_WITH_LOGS
else
DEBUGFLAGS = -O3 -D NDEBUG #-fwhole-program
endif

FLAGS        = #-std=gnu99 -Iinclude
CFLAGS       = -I ../core -I ../examples/shared -fPIC -g -DLWM2M_SERVER_MODE -DLWM2M_LITTLE_ENDIAN -std=gnu99 $(DEBUGFLAGS)
LDFLAGS      = -shared

TARGET  = ../build/libwakaama.so
SOURCES = $(shell echo ../core/*.c ../examples/shared/*.c ../core/er-coap-13/*.c *.c)
HEADERS = $(shell echo ../core/*.h ../examples/shared/*.h ../core/er-coap-13/*.h *.h)
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
