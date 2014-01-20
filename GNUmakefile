
DESTDIR ?= 
PREFIX  ?= /
COMPRESS_MAN ?= yes
STRIP_BINARY ?= yes
#EXAMPLES ?= yes

STDFLAG ?= -D_GNU_SOURCE
CSECFLAGS ?= -fstack-protector-all -Wall --param ssp-buffer-size=4 -D_FORTIFY_SOURCE=2 -fstack-check -DPARANOID
CFLAGS ?= -march=native -pipe -O2 -std=gnu99 
CFLAGS += $(CSECFLAGS) $(STDFLAG)
DEBUGCFLAGS ?= -pipe -Wall -Werror -ggdb3 -Wno-error=unused-variable $(CSECFLAGS)

LDSECFLAGS ?= -Xlinker -zrelro
LDFLAGS += $(LDSECFLAGS)

INSTDIR = $(DESTDIR)$(PREFIX)

objs=\
main.o\
xmalloc.o\

binary=lsb2rcconf

#.PHONY: doc

all: $(objs)
	$(CC) $(CFLAGS) $(LDFLAGS) $(objs) $(LIBS) -o $(binary)

%.o: %.c
	$(CC) $(CFLAGS) $(INC) $< -c -o $@

clean:
	rm -f $(binary) $(objs)

distclean: clean

install:
	install -d "$(INSTDIR)/lib/rc/bin" "$(INSTDIR)/share/man/man1"
ifeq ($(STRIP_BINARY),yes)
	strip --strip-unneeded -R .comment -R .GCC.command.line -R .note.gnu.gold-version $(binary)
endif
	install -m 755 $(binary) "$(INSTDIR)"/lib/rc/bin
#	install -m 644 man/man1/lsb2rcconf.1 "$(INSTDIR)"/share/man/man1/
#ifeq ($(COMPRESS_MAN),yes)
#	rm -f "$(INSTDIR)"/share/man/man1/lsb2rcconf.1.gz
#	gzip "$(INSTDIR)"/share/man/man1/lsb2rcconf.1
#endif

deinstall:
	rm -f "$(INSTDIR)"/lib/rc/lsb2rcconf "$(INSTDIR)"/share/man/man1/lsb2rcconf.1{,.gz}

dpkg: clean
	tar --exclude "debian" --exclude-vcs -C .. -cJvf ../openrc-lsb_0.0.orig.tar.xz openrc-lsb
	dpkg-buildpackage -rfakeroot

