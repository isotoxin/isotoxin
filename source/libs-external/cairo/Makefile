# Makefile for cairo
# (C) 2017 Rotkaermota
# this is Isotoxin minized cairo library

LIBNAME = libcairostatic

CC=gcc
CFLAGS=-O3 -mssse3 -DHAVE_CONFIG_H -DPIXMAN_NO_TLS -DCAIRO_NO_MUTEX=1 -DHAVE_STDINT_H -DHAVE_UINT64_T=1 -I. -Ipixman
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
srcs1 = pixman.c pixman-access.c pixman-access-accessors.c pixman-bits-image.c pixman-combine-float.c pixman-combine32.c pixman-conical-gradient.c pixman-edge.c pixman-edge-accessors.c pixman-fast-path.c pixman-filter.c pixman-general.c pixman-glyph.c pixman-gradient-walker.c pixman-image.c pixman-implementation.c pixman-linear-gradient.c pixman-matrix.c pixman-mmx.c pixman-noop.c pixman-radial-gradient.c pixman-region16.c pixman-region32.c pixman-solid-fill.c pixman-sse2.c pixman-ssse3.c pixman-timer.c pixman-trap.c pixman-utils.c pixman-x86.c
srcs2 = cairo-analysis-surface.c cairo-arc.c cairo-array.c cairo-atomic.c cairo-bentley-ottmann-rectangular.c cairo-bentley-ottmann-rectilinear.c cairo-bentley-ottmann.c \
cairo-boxes-intersect.c cairo-boxes.c cairo-cache.c cairo-cff-subset.c cairo-clip-boxes.c cairo-clip-polygon.c cairo-clip-region.c \
cairo-clip-surface.c cairo-clip-tor-scan-converter.c cairo-clip.c cairo-color.c cairo-composite-rectangles.c cairo-compositor.c cairo-contour.c \
cairo-damage.c cairo-debug.c cairo-default-context.c cairo-device.c cairo-error.c cairo-fallback-compositor.c \
cairo-freelist.c cairo-gstate.c cairo-hash.c cairo-hull.c cairo-image-compositor.c \
cairo-image-source.c cairo-image-surface.c cairo-line.c cairo-mask-compositor.c cairo-matrix.c cairo-mesh-pattern-rasterizer.c cairo-misc.c \
cairo-mono-scan-converter.c cairo-mutex.c cairo-no-compositor.c cairo-observer.c cairo-output-stream.c cairo-paginated-surface.c cairo-path-bounds.c \
cairo-path-fill.c cairo-path-fixed.c cairo-path-in-fill.c cairo-path-stroke-boxes.c cairo-path-stroke-polygon.c cairo-path-stroke-traps.c cairo-path-stroke-tristrip.c \
cairo-path-stroke.c cairo-path.c cairo-pattern.c cairo-pen.c cairo-polygon-intersect.c cairo-polygon-reduce.c cairo-polygon.c cairo-raster-source-pattern.c \
cairo-recording-surface.c cairo-rectangle.c cairo-rectangular-scan-converter.c cairo-region.c cairo-shape-mask-compositor.c \
cairo-slope.c cairo-spans-compositor.c cairo-spans.c cairo-spline.c cairo-stroke-dash.c cairo-stroke-style.c cairo-surface-offset.c cairo-surface-snapshot.c \
cairo-surface-wrapper.c cairo-surface.c cairo-tor-scan-converter.c cairo-tor22-scan-converter.c cairo-traps-compositor.c cairo-traps.c \
cairo-tristrip.c cairo-version.c cairo-wideint.c cairo.c
srcs3 = pixman-stub.c


objs1 = $(srcs1:.c=.o)
objs2 = $(srcs2:.c=.o)
objs3 = $(srcs3:.c=.o)
fsrcs1 = $(addprefix ./pixman/, $(srcs1))
fsrcs2 = $(addprefix ./src/, $(srcs2))
fsrcs3 = $(srcs3)

objs = $(objs1) $(objs2) $(objs3)

all: mkdirs static

static: $(libs)

$(LIBNAME).a: $(objs)
	$(AR) $(ARFLAGS) $(libdir)/$@ $(addprefix $(garbage)/, $(objs))
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

mkdirs:
	mkdir -p $(garbage)
	mkdir -p $(libdir)

$(objs1): $(fsrcs1)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./pixman/$(@:.o=.c)

$(objs2): $(fsrcs2)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./src/$(@:.o=.c)

$(objs3): $(fsrcs3)
	$(CC) -o $(garbage)/$@ -c $(CFLAGS) ./$(@:.o=.c)

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)


