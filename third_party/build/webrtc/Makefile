include ../../../build.mak
include ../../../build/common.mak
include ../os-$(OS_NAME).mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export WEBRTC_LIB := libwebrtc-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export WEBRTC_SONAME := libwebrtc.$(SHLIB_SUFFIX)
export WEBRTC_SHLIB := $(WEBRTC_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../webrtc/src       \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) $(_CFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export WEBRTC_SRCDIR = ../../webrtc/src/webrtc/
export WEBRTC_OBJS = \
	modules/audio_processing/aec/aec_core.o                    \
	modules/audio_processing/aec/aec_rdft.o                    \
	modules/audio_processing/aec/aec_resampler.o               \
	modules/audio_processing/aec/echo_cancellation.o           \
	modules/audio_processing/aecm/aecm_core.o                  \
	modules/audio_processing/aecm/echo_control_mobile.o        \
	modules/audio_processing/ns/noise_suppression.o            \
	modules/audio_processing/ns/noise_suppression_x.o          \
	modules/audio_processing/ns/ns_core.o                      \
	modules/audio_processing/ns/nsx_core.o                     \
	modules/audio_processing/utility/delay_estimator_wrapper.o \
	modules/audio_processing/utility/delay_estimator.o         \
	common_audio/fft4g.o                                       \
	common_audio/ring_buffer.o                                 \
	common_audio/signal_processing/complex_bit_reverse.o       \
	common_audio/signal_processing/complex_fft.o               \
	common_audio/signal_processing/copy_set_operations.o       \
	common_audio/signal_processing/cross_correlation.o         \
	common_audio/signal_processing/division_operations.o       \
	common_audio/signal_processing/downsample_fast.o           \
	common_audio/signal_processing/energy.o                    \
	common_audio/signal_processing/get_scaling_square.o        \
	common_audio/signal_processing/min_max_operations.o        \
	common_audio/signal_processing/randomization_functions.o   \
	common_audio/signal_processing/real_fft.o                  \
	common_audio/signal_processing/spl_init.o                  \
	common_audio/signal_processing/spl_sqrt.o                  \
	common_audio/signal_processing/spl_sqrt_floor.o            \
	common_audio/signal_processing/vector_scaling_operations.o \
	$(WEBRTC_SRC)


export WEBRTC_CFLAGS = $(_CFLAGS) $(WEBRTC_OTHER_CFLAGS)
export WEBRTC_CXXFLAGS = $(WEBRTC_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(WEBRTC_LIB) $(WEBRTC_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(WEBRTC_LIB) $(WEBRTC_SONAME)

libwebrtc: $(WEBRTC_LIB)
$(WEBRTC_SONAME): $(WEBRTC_LIB)
$(WEBRTC_LIB) $(WEBRTC_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC app=libwebrtc $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC app=libwebrtc $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.webrtc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC app=libwebrtc $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC app=libwebrtc $@
