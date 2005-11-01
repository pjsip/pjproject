include ../../pjlib/build/make-$(TARGET).inc

PA_DIR := portaudio
PA_CFLAGS := -DPA_NO_ASIO -DPA_NO_DS -I$(PA_DIR) -I../src/pjmedia/portaudio

_CFLAGS := $(_CFLAGS) -I../src -I../../pjlib/src $(PA_CFLAGS)
_LDFLAGS := -L../../pjlib/lib -lpjmedia $(_LDFLAGS) -lwinmm


export PA_SOURCES := 	$(PA_DIR)/pa_win_hostapis.c \
			$(PA_DIR)/pa_win_util.c \
			$(PA_DIR)/pa_win_wmme.c
#			$(PA_DIR)/pa_x86_plain_converters.c \

export PJMEDIA_SOURCES := $(PA_SOURCES)
export PJMEDIA_CFLAGS := $(_CFLAGS)
export PJMEDIA_LIB := ../lib/libpjmedia.a
export PJMEDIA_EXTRA_DEP := ../../pjlib/build/$(PJLIB_LIB)

export TEST_EXE := ../bin/test_mingw.exe
export TEST_SOURCES := 
export TEST_CFLAGS := $(_CFLAGS)
export TEST_LDFLAGS := $(_LDFLAGS)
export TEST_EXTRA_DEP := $(PJMEDIA_LIB)

export PJAUT_EXE := ../bin/pjaut_mingw.exe
export PJAUT_SOURCES := 
export PJAUT_CFLAGS = $(_CFLAGS)
export PJAUT_LDFLAGS := $(_LDFLAGS)
export PJAUT_EXTRA_DEP := $(PJMEDIA_LIB)
