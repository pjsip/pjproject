#
# make-linux-i386.inc: Linux i386 specific compilation switches.
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


_CFLAGS := $(CFLAGS) -Wall -O2 -I../src -DLINUX -DPJ_GUID_TYPE=PJ_GUID_SIMPLE \
	  $(PJLIB_OPTIMIZE)
_CXXFLAGS := $(_CFLAGS) $(CXXFLAGS)
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
EXE :=

export PJLIB_SOURCES := os_unix.c
export PJLIB_CFLAGS := $(_CFLAGS)
export PJLIB_LIB := ../lib/libpj.a

export TEST_SOURCES: = 
export TEST_CXXFLAGS := $(PJLIB_CFLAGS)
export TEST_LDFLAGS := -L../lib -lpj -lpthread -lstdc++
export TEST_EXE := ../bin/test

