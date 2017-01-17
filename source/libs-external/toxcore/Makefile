# Makefile for toxcore
# (C) 2016 Rotkaermota

LIBNAME = libtoxcorestatic

CC=gcc
CFLAGS=-O3 -m64 -include config.h -D_NIX
#CFLAGS=-O3  -D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN
#CFLAGS=-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS=-g -DDEBUG
#CFLAGS=-O3 -Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#           -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS= 
LDSHARED=gcc
CPP=gcc -E

AR=ar
ARFLAGS=rc
RANLIB=ranlib
SHELL=/bin/sh

garbage =$(GARBAGE)/__garbage/$(LIBNAME)
libdir =$(GARBAGE)/__libs

libs = $(LIBNAME).a

idirs = vs /usr/include/opus
I = $(addprefix -I, $(idirs))


srcs = toxav/audio.c \
toxav/bwcontroller.c \
toxav/groupav.c \
toxav/msi.c \
toxav/ring_buffer.c \
toxav/rtp.c \
toxav/toxav.c \
toxav/toxav_old.c \
toxav/video.c \
toxcore/crypto_core.c \
toxcore/DHT.c \
toxcore/friend_connection.c \
toxcore/friend_requests.c \
toxcore/group.c \
toxcore/LAN_discovery.c \
toxcore/list.c \
toxcore/logger.c \
toxcore/Messenger.c \
toxcore/network.c \
toxcore/net_crypto.c \
toxcore/onion.c \
toxcore/onion_announce.c \
toxcore/onion_client.c \
toxcore/ping.c \
toxcore/ping_array.c \
toxcore/TCP_client.c \
toxcore/TCP_connection.c \
toxcore/TCP_server.c \
toxcore/tox.c \
toxcore/util.c \
toxdns/toxdns.c


srcnames = $(notdir $(srcs))
objs = $(srcnames:.c=.o)

all: mkdirs static

static: $(libs)

$(LIBNAME).a: allobjs
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(objs))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

allobjs: ;
	@for f in $(srcs); do \
    fn="$${f##*/}"; \
    fno="$(garbage)/$${fn%.*}.o"; \
    echo CC $$f; \
    $(CC) -o $$fno $(I) -c $(CFLAGS) $$f; \
    done

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)

print-%  : ; @echo $* = $($*)
