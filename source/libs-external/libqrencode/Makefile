# Makefile for pnglib
# (C) 2016 Rotkaermota

LIBNAME = libqrencode

CC=gcc
CFLAGS=-O3 -m64 -DHAVE_HIDDEN -DSTATIC_IN_RELEASE=static -DMAJOR_VERSION=0 -DMINOR_VERSION=0 -DMICRO_VERSION=0 -DVERSION=""
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
srcs = bitstream.c mask.c mmask.c mqrspec.c qrencode.c qrinput.c qrspec.c rsecc.c split.c
objs = $(srcs:.c=.o)
fsrcs = $(addprefix ./src/, $(srcs))

all: mkdirs static

static: $(libs)

$(LIBNAME).a: $(objs)
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(objs))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

$(objs): $(fsrcs)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/$(@:.o=.c)

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)


