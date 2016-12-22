TARGET = diary
SRC = diary.c
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

CC = gcc
CFLAGS = -Wall
CPPFLAGS += -D_XOPEN_SOURCE           # for wcwidth
CPPFLAGS += -D_XOPEN_SOURCE_EXTENDED  # for waddnwstr

UNAME = ${shell uname}

ifeq ($(UNAME),FreeBSD)
	LDLIBS = -lncurses
endif

ifeq ($(UNAME),Linux)
	LDLIBS = -lncursesw
endif

ifeq ($(UNAME),Darwin)
	LDLIBS = -lncurses -framework CoreFoundation
endif


default: $(TARGET)

$(TARGET): $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
