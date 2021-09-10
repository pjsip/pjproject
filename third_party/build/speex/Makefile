include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export SPEEX_LIB := libspeex-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export SPEEX_SONAME := libspeex.$(SHLIB_SUFFIX)
export SPEEX_SHLIB := $(SPEEX_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../speex/include \
		   $(CC_INC)../../speex/libspeex \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS) 

export SPEEX_SRCDIR = ../../speex/libspeex
export SPEEX_OBJS = 	bits.o cb_search.o exc_10_16_table.o  \
			exc_10_32_table.o exc_20_32_table.o \
			exc_5_256_table.o exc_5_64_table.o \
			exc_8_128_table.o fftwrap.o filterbank.o \
			filters.o gain_table.o gain_table_lbr.o \
			hexc_10_32_table.o hexc_table.o \
			high_lsp_tables.o \
			kiss_fft.o kiss_fftr.o lpc.o \
			lsp.o lsp_tables_nb.o ltp.o \
			mdf.o modes.o modes_wb.o \
			nb_celp.o preprocess.o \
			quant_lsp.o resample.o sb_celp.o smallft.o \
			speex.o speex_callbacks.o speex_header.o \
			scal.o stereo.o vbr.o vq.o window.o

export SPEEX_CFLAGS = -DHAVE_CONFIG_H $(_CFLAGS)
export SPEEX_LDFLAGS := $(PJLIB_LDLIB) $(_LDFLAGS)

export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(SPEEX_LIB) $(SPEEX_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(SPEEX_LIB) $(SPEEX_SONAME)

libspeex: $(SPEEX_LIB)
$(SPEEX_SONAME): $(SPEEX_LIB)
$(SPEEX_LIB) $(SPEEX_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $@


