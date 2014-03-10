# @configure_input@
include ../../../build.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export PORTAUDIO_LIB:=libportaudio-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export PORTAUDIO_SONAME := libportaudio.$(SHLIB_SUFFIX)
export PORTAUDIO_SHLIB := $(PORTAUDIO_SONAME).$(PJ_VERSION_MAJOR)
endif

PORTAUDIO_OBJS += pa_allocation.o \
		  pa_converters.o \
		  pa_cpuload.o \
		  pa_dither.o \
		  pa_debugprint.o \
		  pa_front.o \
		  pa_process.o \
		  pa_skeleton.o \
		  pa_stream.o \
		  pa_trace.o


###############################################################################
# Gather all flags.
#
export _CFLAGS 	+= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)src 
export _CXXFLAGS+= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS += $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PORTAUDIO library
#
export PORTAUDIO_SRCDIR = src
export PORTAUDIO_OBJS += 
export PORTAUDIO_CFLAGS += $(_CFLAGS)
export PORTAUDIO_LDFLAGS += $(_LDFLAGS)

export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(PORTAUDIO_LIB) $(PORTAUDIO_SONAME)

all: $(TARGETS)

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(PORTAUDIO_LIB) $(PORTAUDIO_SONAME)

libportaudio: $(PORTAUDIO_LIB)
$(PORTAUDIO_SONAME): $(PORTAUDIO_LIB)
$(PORTAUDIO_LIB) $(PORTAUDIO_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean:
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.libportaudio-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $@

