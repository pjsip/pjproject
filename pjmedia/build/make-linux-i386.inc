include ../../pjlib/build/make-$(TARGET).inc

PA_DIR := portaudio
PA_CFLAGS := 	-DPA_LITTLE_ENDIAN -DPA_USE_OSS \
		-I$(PA_DIR) -I../src/pjmedia/portaudio

_CFLAGS := $(_CFLAGS) -I../src -I../../pjlib/src $(PA_CFLAGS)
_LDFLAGS := $(_LDFLAGS) -L../lib -L../../pjlib/lib \
		      -lpjmedia -lpj -lpthread

export PA_SOURCES :=	$(PA_DIR)/pa_unix_hostapis.c \
			$(PA_DIR)/pa_unix_util.c \
			$(PA_DIR)/pa_unix_oss.c
#			$(PA_DIR)/pa_linux_alsa.c


export PJMEDIA_SOURCES = $(PA_SOURCES)
export PJMEDIA_CFLAGS = $(_CFLAGS) 
export PJMEDIA_LIB = ../lib/libpjmedia.a
export PJMEDIA_EXTRA_DEP := ../../pjlib/build/$(PJLIB_LIB)

export TEST_EXE := ../bin/test_linux
export TEST_SOURCES := 
export TEST_CFLAGS := $(PJMEDIA_CFLAGS)
export TEST_LDFLAGS := $(_LDFLAGS)
export TEST_EXTRA_DEP := $(PJMEDIA_LIB)

export PJAUT_EXE := ../bin/pjaut_linux
export PJAUT_SOURCES := 
export PJAUT_CFLAGS := $(PJMEDIA_CFLAGS)
export PJAUT_LDFLAGS = $(_LDFLAGS)
export PJAUT_EXTRA_DEP := $(PJMEDIA_LIB)
