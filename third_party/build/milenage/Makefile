include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export MILENAGE_LIB := libmilenage-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export MILENAGE_SONAME := libmilenage.$(SHLIB_SUFFIX)
export MILENAGE_SHLIB := $(MILENAGE_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../milenage/include \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export MILENAGE_SRCDIR = ../../milenage
export MILENAGE_OBJS = milenage.o rijndael.o
export MILENAGE_CFLAGS = $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(MILENAGE_LIB) $(MILENAGE_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(MILENAGE_LIB) $(MILENAGE_SONAME)

libmilenage: $(MILENAGE_LIB)
$(MILENAGE_SONAME): $(MILENAGE_LIB)
$(MILENAGE_LIB) $(MILENAGE_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=MILENAGE app=libmilenage $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=MILENAGE app=libmilenage $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=MILENAGE app=libmilenage $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=MILENAGE app=libmilenage $@


