include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export YUV_LIB := libyuv-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export YUV_SONAME := libyuv.$(SHLIB_SUFFIX)
export YUV_SHLIB := $(YUV_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../yuv/include \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) $(_CFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export YUV_SRCDIR = ../../yuv/source
export YUV_OBJS = \
	compare.o           \
	compare_common.o    \
	compare_gcc.o       \
	compare_neon64.o    \
	compare_neon.o      \
	compare_win.o       \
	convert_argb.o      \
	convert.o           \
	convert_from_argb.o \
	convert_from.o      \
	convert_jpeg.o      \
	convert_to_argb.o   \
	convert_to_i420.o   \
	cpu_id.o            \
	mjpeg_decoder.o     \
	mjpeg_validate.o    \
	planar_functions.o  \
	rotate_any.o        \
	rotate_argb.o       \
	rotate.o            \
	rotate_common.o     \
	rotate_gcc.o        \
	rotate_dspr2.o      \
	rotate_neon64.o     \
	rotate_neon.o       \
	rotate_win.o        \
	row_any.o           \
	row_common.o        \
	row_gcc.o           \
	row_dspr2.o         \
	row_neon64.o        \
	row_neon.o          \
	row_win.o           \
	scale_any.o         \
	scale_argb.o        \
	scale.o             \
	scale_common.o      \
	scale_gcc.o         \
	scale_dspr2.o       \
	scale_neon64.o      \
	scale_neon.o        \
	scale_win.o         \
	video_common.o

export YUV_CFLAGS = -fomit-frame-pointer -fno-strict-aliasing -Wno-memset-elt-size -Wno-unknown-warning-option -Wno-pragmas $(_CFLAGS)
export YUV_CXXFLAGS = -fomit-frame-pointer -fno-strict-aliasing -Wno-memset-elt-size -Wno-unknown-warning-option -Wno-pragmas $(_CXXFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(YUV_LIB) $(YUV_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(YUV_LIB) $(YUV_SONAME)

libyuv: $(YUV_LIB)
$(YUV_SONAME): $(YUV_LIB)
$(YUV_LIB) $(YUV_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=YUV app=libyuv $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=YUV app=libyuv $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.yuv-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=YUV app=libyuv $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=YUV app=libyuv $@
