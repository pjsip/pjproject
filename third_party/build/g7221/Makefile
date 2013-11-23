include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export G7221_CODEC_LIB := libg7221codec-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export G7221_CODEC_SONAME := libg7221codec.$(SHLIB_SUFFIX)
export G7221_CODEC_SHLIB := $(G7221_CODEC_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../.. $(CC_INC)../../g7221/common \
		   $(CC_INC)../../g7221/common/stl-files \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export G7221_CODEC_SRCDIR = ../../g7221
export G7221_CODEC_OBJS = common/common.o common/huff_tab.o common/tables.o \
                	common/basic_op.o  \
                	decode/coef2sam.o decode/dct4_s.o decode/decoder.o \
                	encode/dct4_a.o encode/sam2coef.o encode/encoder.o

export G7221_CODEC_CFLAGS = $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(G7221_CODEC_LIB) $(G7221_CODEC_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(G7221_CODEC_LIB) $(G7221_CODEC_SONAME)

libg7221codec: $(G7221_CODEC_LIB)
$(G7221_CODEC_SONAME): $(G7221_CODEC_LIB)
$(G7221_CODEC_LIB) $(G7221_CODEC_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=G7221_CODEC app=libg7221codec $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=G7221_CODEC app=libg7221codec $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=G7221_CODEC app=libg7221codec $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=G7221_CODEC app=libg7221codec $@


