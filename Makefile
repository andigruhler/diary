TARGET = diary
SRC = diary.c
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

CC = gcc
CFLAGS = -Wall
UNAME = ${shell uname}

ifeq ($(UNAME),Linux)
	LIBS = -lncursesw
endif

ifeq ($(UNAME),Darwin)
	LIBS = -lncurses
endif


default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
