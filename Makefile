TARGET = diary
SRCDIR = src/
_SRC = utils.c caldav.c diary.c
SRC = $(addprefix $(SRCDIR), $(_SRC))
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

MANDIR := $(DESTDIR)$(PREFIX)/share/man
MAN1 = man/diary.1

CC = gcc
CFLAGS = -Wall \
         -DGOOGLE_OAUTH_CLIENT_ID=\"$(GOOGLE_OAUTH_CLIENT_ID)\" \
         -DGOOGLE_OAUTH_CLIENT_SECRET=\"$(GOOGLE_OAUTH_CLIENT_SECRET)\"
UNAME = ${shell uname}

ifeq ($(UNAME),FreeBSD)
	LIBS = -lncurses -lcurl -lpthread
endif

ifeq ($(UNAME),Linux)
	LIBS = -lncursesw -lcurl -lpthread
endif

ifeq ($(UNAME),Darwin)
	LIBS = -lncurses -lcurl -lpthread -framework CoreFoundation
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
