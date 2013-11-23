include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../resample/include \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export RESAMPLE_SRCDIR = ../../resample/src
export RESAMPLE_OBJS = resamplesubs.o
export RESAMPLE_CFLAGS = $(_CFLAGS)

export RESAMPLE_LIB := libresample-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export RESAMPLE_SONAME := libresample.$(SHLIB_SUFFIX)
export RESAMPLE_SHLIB := $(RESAMPLE_SONAME).$(PJ_VERSION_MAJOR)
endif

export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(RESAMPLE_LIB) $(RESAMPLE_SONAME)
ifneq ($(PJ_SHARED_LIBRARIES),)
else
ifneq ($(PJ_RESAMPLE_DLL),)
export RESAMPLE_CFLAGS := -fPIC $(RESAMPLE_CFLAGS)
endif
endif

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(RESAMPLE_LIB) $(RESAMPLE_SONAME)

dep: depend

libresample: $(RESAMPLE_LIB)
$(RESAMPLE_SONAME): $(RESAMPLE_LIB)
$(RESAMPLE_LIB) $(RESAMPLE_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $@

realclean:
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $@


