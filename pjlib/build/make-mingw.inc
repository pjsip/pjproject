#
# make-mingw.inc: Mingw specific compilation switches.
#

#
# PJLIB_OPTIMIZE variable.
#
ifdef MINSIZE
PJLIB_OPTIMIZE := -DPJ_HAS_TCP=0 -DPJ_LOG_MAX_LEVEL=0 -DPJ_HAS_THREADS=0 \
		  -DPJ_FUNCTIONS_ARE_INLINED=0
else
PJLIP_OPTIMIZE :=
endif


_CFLAGS := $(CFLAGS) -Wall -O3 -I../src -DWIN32 -D_WIN32 \
	   -DPJ_GUID_TYPE=PJ_GUID_COCREATEGUID $(PJLIB_OPTIMIZE)
_CXXFLAGS := $(_CFLAGS) $(CXXFLAGS)
_LDFLAGS := -L../lib -lpj -lwsock32 -lws2_32 -lstdc++ -lole32 $(LDFLAGS)
CCOUT := -o
CC := gcc -c
AR := ar r
MV := mv
RM := rm -f
RMDIR := rm -rf
MKDIR := mkdir 
OBJEXT := .o
LD := gcc
LDOUT := -o
RANLIB := ranlib
EXE := .exe

export PJLIB_SOURCES := os_win32.c ioqueue_winnt.c
export PJLIB_CFLAGS := $(_CFLAGS)
export PJLIB_LIB := ../lib/libpj.a

export TEST_SOURCES = 
export TEST_CXXFLAGS = $(_CXXFLAGS)
export TEST_LDFLAGS = $(_LDFLAGS)
export TEST_EXE = ../bin/test.exe

