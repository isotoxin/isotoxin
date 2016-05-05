# Makefile for zlib and minizip
# (C) 2016 Rotkaermota

CC=gcc
CFLAGS=-O3  -DHAVE_HIDDEN
#CFLAGS=-O3  -D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN
#CFLAGS=-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS=-g -DDEBUG
#CFLAGS=-O3 -Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#           -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS= 
LDSHARED=gcc
CPP=gcc -E

LIBS=zlib.a minizip.a

AR=ar
ARFLAGS=rc
RANLIB=ranlib
SHELL=/bin/sh

garbage =$(GARBAGE)/__garbage/zlib
libdir =$(GARBAGE)/__libs

mzobjs = ioapi.o mztools.o unzip.o zip.o

OBJZ = adler32.o crc32.o deflate.o infback.o inffast.o inflate.o inftrees.o trees.o zutil.o
OBJG = compress.o uncompr.o gzclose.o gzlib.o gzread.o gzwrite.o
OBJC = $(OBJZ) $(OBJG)

# to use the asm code: make OBJA=match.o, PIC_OBJA=match.lo
OBJA =

OBJS = $(OBJC) $(OBJA)

all: mkdirs static

static: $(LIBS)

zlib.a: $(OBJS)
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(OBJS))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

minizip.a: $(mzobjs)
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(mzobjs))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

match.o: match.S
	$(CPP) match.S > _match.s
	$(CC) -c _match.s
	mv _match.o match.o
	rm -f _match.s

$(OBJS): 
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/$(@:.o=.c)

$(mzobjs):
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/contrib/minizip/$(@:.o=.c) -I./src

clean:
	rm -f $(libdir)/zlib.a
	rm -f $(libdir)/minizip.a
	rm -r -f $(garbage)/zlib


