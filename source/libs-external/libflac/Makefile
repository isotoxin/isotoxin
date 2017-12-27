# Makefile for cairo
# (C) 2017 Rotkaermota

LIBNAME = libflac

CC=gcc

CFLAGS=-O3 -mssse3 -DFLAC__HAS_OGG=0 -DFLAC__NO_DLL -DFLaC__INLINE=inline -DHAVE_CONFIG_H -DFLAC__CPU_X86_64 -DFLAC__HAS_X86INTRIN -DFLAC__ALIGN_MALLOC_DATA -DVERSION=\"1.3.1\" -DFLAC__NO_DLL -DFLAC__OVERFLOW_DETECT -DHAVE_LROUND -I. -Iinclude -Isrc/libFLAC/include -I../libogg -I../libogg/include
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
#srcs1 = win_utf8_io.c
srcs2 = bitmath.c bitreader.c bitwriter.c cpu.c crc.c fixed.c fixed_intrin_sse2.c fixed_intrin_ssse3.c float.c \
    format.c lpc.c lpc_intrin_sse.c lpc_intrin_sse2.c lpc_intrin_sse41.c md5.c memory.c metadata_iterators.c metadata_object.c \
    stream_decoder.c stream_encoder.c stream_encoder_framing.c stream_encoder_intrin_sse2.c stream_encoder_intrin_ssse3.c window.c


#objs1 = $(srcs1:.c=.o)
objs2 = $(srcs2:.c=.o)
#objs3 = $(srcs3:.c=.o)
#fsrcs1 = $(addprefix ./src/share/, $(srcs1))
fsrcs2 = $(addprefix ./src/libFLAC/, $(srcs2))
#fsrcs3 = $(srcs3)

#objs = $(objs1) $(objs2) $(objs3)
objs = $(objs2)

all: mkdirs static

static: $(libs)

$(LIBNAME).a: $(objs)
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(objs))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

#$(objs1): $(fsrcs1)
#	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/share/$(@:.o=.c)

$(objs2): $(fsrcs2)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/libFLAC/$(@:.o=.c)

#$(objs3): $(fsrcs3)
#	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./$(@:.o=.c)

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)


