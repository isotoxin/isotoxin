# Makefile for jpeg
# (C) 2016 Rotkaermota

LIBNAME = libjpegstatic

CC=gcc
CFLAGS=-O3 -m64 -DHAVE_HIDDEN -D_JPEGLIB_
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

srcs = cdjpeg.c jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c jmemmgr.c jmemnobs.c jquant1.c jquant2.c jutils.c rdbmp.c rdcolmap.c rdgif.c rdppm.c rdrle.c rdswitch.c rdtarga.c transupp.c wrbmp.c wrgif.c wrppm.c wrrle.c wrtarga.c
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
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/$(@:.o=.c) -I.

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)


