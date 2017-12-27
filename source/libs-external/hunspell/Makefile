# Makefile for hunspell
# (C) 2017 Rotkaermota

LIBNAME = libhunspell

CC=gcc
CFLAGS=--std=c++11 -O3 -I./ -I../ -include ./config.h
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
srcs = affentry.cxx affixmgr.cxx csutil.cxx hashmgr.cxx hunspell.cxx phonet.cxx replist.cxx suggestmgr.cxx
objs = $(srcs:.cxx=.o)
fsrcs = $(addprefix ./src/, $(srcs))

all: mkdirs static

static: $(libs)

$(LIBNAME).a: $(objs) hunspell_io.o
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(objs)) $(garbage)/hunspell_io.o
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

$(objs): $(fsrcs)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/$(@:.o=.cxx)

hunspell_io.o: ./hunspell_io.cpp
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./hunspell_io.cpp

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)


