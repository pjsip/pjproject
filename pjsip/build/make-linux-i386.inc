#
# make-linux-i386.inc: Platform specific rules for Linux i386 compile.
#

#
# Include PJLIB settings.
#
include ../../pjlib/build/make-$(TARGET).inc

#
# make-optimize.inc declares PJSIP_OPTIMIZE variable
#
include make-optimize.inc



_CFLAGS := $(_CFLAGS) -I../../pjlib/src -I../../pjmedia/src \
          -I../src $(PJSIP_OPTIMIZE)
_LDFLAGS := $(_LDFLAGS) -L../lib -L../../pjlib/lib  \
		      -L../../pjmedia/lib -lpjsip_core -lpjsip_ua -lpjsip_simple \
		      -lpjmedia -lpj -lpthread

#
# libpjsip_core.a
#
export PJSIP_SOURCES = 
export PJSIP_CFLAGS = $(_CFLAGS) 
export PJSIP_LIB = ../lib/libpjsip_core.a
export PJSIP_EXTRA_DEP := 

#
# libpjsip_ua.a
#
export PJSIP_UA_SOURCES = 
export PJSIP_UA_CFLAGS = $(_CFLAGS) 
export PJSIP_UA_LIB = ../lib/libpjsip_ua.a
export PJSIP_UA_EXTRA_DEP :=

#
# libpjsip_simple.a
#
export PJSIP_SIMPLE_SOURCES :=
export PJSIP_SIMPLE_CFLAGS := $(_CFLAGS)
export PJSIP_SIMPLE_LIB := ../lib/libpjsip_simple.a
export PJSIP_SIMPLE_EXTRA_LIB :=

#
# pjsua.exe
#
export PJSUA_SOURCES = 
export PJSUA_CFLAGS = $(_CFLAGS)
export PJSUA_LDFLAGS =  $(_LDFLAGS)
export PJSUA_EXE = ../bin/pjsua
export PJSUA_EXTRA_DEP := ../lib/libpjsip_core.a ../lib/libpjsip_ua.a \
			  ../lib/libpjsip_simple.a ../../pjlib/lib/libpj.a \
			  ../../pjmedia/lib/libpjmedia.a


