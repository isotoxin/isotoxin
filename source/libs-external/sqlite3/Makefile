# Makefile for sqlite
# (C) 2016 Rotkaermota

LIBNAME = libsqlitestatic

CC=gcc
CFLAGS=-O3 -DNDEBUG -DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_HAS_CODEC=1 -D_HAVE_SQLITE_CONFIG_H
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
srcs = sqlite3.c
objs = $(srcs:.c=.o)

all: mkdirs static

static: $(libs)

$(LIBNAME).a: $(objs)
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(objs))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

$(objs): $(srcs)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) $(@:.o=.c)

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)


