# Makefile for filteraudio
# (C) 2016 Rotkaermota

LIBNAME = libfilteraudio

CC=gcc
CFLAGS=-O3
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

srcs = aec/aec_core.c \
aec/aec_core_sse2.c \
aec/aec_rdft.c \
aec/aec_rdft_sse2.c \
aec/aec_resampler.c \
aec/echo_cancellation.c \
agc/analog_agc.c \
agc/digital_agc.c \
filter_audio.c \
ns/noise_suppression.c \
ns/noise_suppression_x.c \
ns/nsx_core.c \
ns/nsx_core_c.c \
ns/ns_core.c \
other/complex_bit_reverse.c \
other/complex_fft.c \
other/copy_set_operations.c \
other/cross_correlation.c \
other/delay_estimator.c \
other/delay_estimator_wrapper.c \
other/division_operations.c \
other/dot_product_with_scale.c \
other/downsample_fast.c \
other/energy.c \
other/fft4g.c \
other/float_util.c \
other/get_scaling_square.c \
other/high_pass_filter.c \
other/min_max_operations.c \
other/randomization_functions.c \
other/real_fft.c \
other/resample_48khz.c \
other/resample_by_2.c \
other/resample_by_2_internal.c \
other/resample_fractional.c \
other/ring_buffer.c \
other/speex_resampler.c \
other/splitting_filter.c \
other/spl_init.c \
other/spl_sqrt.c \
other/spl_sqrt_floor.c \
other/vector_scaling_operations.c \
vad/vad_core.c \
vad/vad_filterbank.c \
vad/vad_gmm.c \
vad/vad_sp.c \
vad/webrtc_vad.c \
zam/filters.c


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
    $(CC) -o $$fno -c $(CFLAGS) $$f; \
    done

clean:
	rm -f $(libdir)/$(LIBNAME).a
	rm -r -f $(garbage)/$(LIBNAME)

print-%  : ; @echo $* = $($*)
