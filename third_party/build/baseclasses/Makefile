include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export BASECLASSES_LIB := libbaseclasses-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export BASECLASSES_SONAME := libbaseclasses.$(SHLIB_SUFFIX)
export BASECLASSES_SHLIB := $(BASECLASSES_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../../BaseClasses \
		   $(CC_INC)../../../pjmedia/include $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export BASECLASSES_SRCDIR = ../../BaseClasses
export BASECLASSES_OBJS = amfilter.o amvideo.o arithutil.o combase.o ctlutil.o \
                          mtype.o renbase.o wxdebug.o wxlist.o \
                          wxutil.o 

export BASECLASSES_CFLAGS = $(_CFLAGS)
export BASECLASSES_CXXFLAGS = -fpermissive -D__STRSAFE__NO_INLINE=1 -DPJMEDIA_VIDEO_DEV_HAS_DSHOW=1 $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(BASECLASSES_LIB) $(BASECLASSES_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(BASECLASSES_LIB) $(BASECLASSES_SONAME)

libbaseclasses: $(BASECLASSES_LIB)
$(BASECLASSES_SONAME): $(BASECLASSES_LIB)
$(BASECLASSES_LIB) $(BASECLASSES_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=BASECLASSES app=baseclasses $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=BASECLASSES app=baseclasses $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=BASECLASSES app=baseclasses $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=BASECLASSES app=baseclasses $@


