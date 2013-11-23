include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export GSM_CODEC_LIB := libgsmcodec-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export GSM_CODEC_SONAME := libgsmcodec.$(SHLIB_SUFFIX)
export GSM_CODEC_SHLIB := $(GSM_CODEC_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../gsm/inc \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export GSM_CODEC_SRCDIR = ../../gsm/src
export GSM_CODEC_OBJS = add.o code.o decode.o \
                	gsm_create.o gsm_decode.o gsm_destroy.o \
                	gsm_encode.o gsm_explode.o gsm_implode.o \
                	gsm_option.o long_term.o \
                	lpc.o preprocess.o rpe.o short_term.o \
                	table.o

export GSM_CODEC_CFLAGS = -DSASR -DWAV49 -DNeedFunctionPrototypes=1 $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(GSM_CODEC_LIB) $(GSM_CODEC_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONE: $(GSM_CODEC_LIB) $(GSM_CODEC_SONAME)

libgsmcodec: $(GSM_CODEC_LIB)
$(GSM_CODEC_SONAME): $(GSM_CODEC_LIB)
$(GSM_CODEC_LIB) $(GSM_CODEC_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=GSM_CODEC app=libgsmcodec $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=GSM_CODEC app=libgsmcodec $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=GSM_CODEC app=libgsmcodec $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=GSM_CODEC app=libgsmcodec $@


