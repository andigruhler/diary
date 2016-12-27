TARGET = diary
SRC = diary.c
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

CC = gcc
CFLAGS = -Wall
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

$(SRC): diary.h

clean:
	$(RM) $(TARGET)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	$(RM) $(BINDIR)/$(TARGET)
