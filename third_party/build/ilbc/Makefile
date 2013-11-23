include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export ILBC_LIB := libilbccodec-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export ILBC_SONAME := libilbccodec.$(SHLIB_SUFFIX)
export ILBC_SHLIB := $(ILBC_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../ilbc \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export ILBC_SRCDIR = ../../ilbc
export ILBC_OBJS = FrameClassify.o LPCdecode.o LPCencode.o \
		   StateConstructW.o StateSearchW.o anaFilter.o \
		   constants.o createCB.o doCPLC.o \
		   enhancer.o filter.o gainquant.o \
		   getCBvec.o helpfun.o hpInput.o \
		   hpOutput.o iCBConstruct.o iCBSearch.o \
		   iLBC_decode.o iLBC_encode.o lsf.o \
		   packing.o syntFilter.o
export ILBC_CFLAGS = $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(ILBC_LIB) $(ILBC_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(ILBC_LIB) $(ILBC_SONAME)

libilbccodec: $(ILBC_LIB)
$(ILBC_SONAME): $(ILBC_LIB)
$(ILBC_LIB) $(ILBC_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=ILBC app=libilbccodec $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=ILBC app=libilbccodec $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=ILBC app=libilbccodec $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=ILBC app=libilbccodec $@


