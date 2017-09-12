TARGET = diary
SRC = diary.c
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

MANDIR := $(DESTDIR)$(PREFIX)/share/man
MAN1 = diary.1

CC = gcc
CFLAGS = -Wall
UNAME = ${shell uname}

ifeq ($(UNAME),FreeBSD)
	LIBS = -lncurses
endif

ifeq ($(UNAME),Linux)
	LIBS = -lncursesw
endif

ifeq ($(UNAME),Darwin)
	LIBS = -lncurses -framework CoreFoundation
endif


default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(BINDIR)/$(TARGET)
	install -d $(MANDIR)/man1
	install -m644 $(MAN1) $(MANDIR)/man1/$(MAN1)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(MANDIR)/man1/$(MAN1)
