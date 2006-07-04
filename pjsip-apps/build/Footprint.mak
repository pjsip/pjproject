#
# This file is used by get-footprint.py script to build samples/footprint.c
# to get the footprint report for PJSIP/PJMEDIA.
#
include ../../build/common.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)
PJSIP_LIB:=../../pjsip/lib/libpjsip-$(TARGET_NAME)$(LIBEXT)
PJSIP_UA_LIB:=../../pjsip/lib/libpjsip-ua-$(TARGET_NAME)$(LIBEXT)
PJSIP_SIMPLE_LIB:=../../pjsip/lib/libpjsip-simple-$(TARGET_NAME)$(LIBEXT)
PJSUA_LIB_LIB=../../pjsip/lib/libpjsua-$(TARGET_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../../pjsip/include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)

export LIBS :=	$(subst /,$(HOST_PSEP),$(PJSUA_LIB_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_UA_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_SIMPLE_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_CODEC_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJLIB_LIB))


export _LDFLAGS := $(LIBS) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) -lm



EXE := footprint.exe

all: 
	$(CC_NAME) -o $(EXE) ../src/samples/footprint.c $(FCFLAGS) $(_CFLAGS) $(_LDFLAGS)

clean:
	rm -f $(EXE)

