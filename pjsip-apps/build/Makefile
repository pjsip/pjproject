include ../../build/common.mak

RULES_MAK := ../../build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)
PJSIP_LIB:=../../pjsip/lib/libpjsip-$(TARGET_NAME)$(LIBEXT)
PJSIP_UA_LIB:=../../pjsip/lib/libpjsip-ua-$(TARGET_NAME)$(LIBEXT)
PJSIP_SIMPLE_LIB:=../../pjsip/lib/libpjsip-simple-$(TARGET_NAME)$(LIBEXT)
PJSUA_LIB_LIB=../../pjsip/lib/libpjsua-$(TARGET_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../../pjsip/include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJSUA_LIB_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJSIP_UA_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJSIP_SIMPLE_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJSIP_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJMEDIA_CODEC_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) -lm


###############################################################################
# Defines for building PJSUA
#
export PJSUA_SRCDIR = ../src/pjsua
export PJSUA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			main.o 
export PJSUA_CFLAGS += $(_CFLAGS)
export PJSUA_LDFLAGS += $(_LDFLAGS)
export PJSUA_EXE:=../bin/pjsua-$(TARGET_NAME)$(HOST_EXE)



###############################################################################
# Defines for building PJSIP-PERF
#
export PJSIPPERF_SRCDIR = ../src/pjsip-perf
export PJSIPPERF_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			 handler_call.o handler_options.o main.o
export PJSIPPERF_CFLAGS += $(_CFLAGS)
export PJSIPPERF_LDFLAGS += $(_LDFLAGS)
export PJSIPPERF_EXE:=../bin/pjsip-perf-$(TARGET_NAME)$(HOST_EXE)



export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
#
TARGETS := pjsua pjsip-perf samples

.PHONY: $(TARGETS)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend pjsua clean realclean distclean

pjsua:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $(PJSUA_EXE)

pjsip-perf:
	$(MAKE) -f $(RULES_MAK) APP=PJSIPPERF app=pjsip-perf $(PJSUA_EXE)

samples:
	$(MAKE) -f Samples.mak

.PHONY: ../lib/pjsua.ko
../lib/pjsua.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $@

clean depend realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIPPERF app=pjsip-perf $@
	$(MAKE) -f Samples.mak $@
	@if test "$@" = "depend"; then \
	  echo '$(PJSUA_EXE): $(PJSIP_LIB) $(PJSIP_UA_LIB) $(PJSIP_SIMPLE) $(PJSUA_LIB_LIB) $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJMEDIA_LIB) $(PJMEDIA_CODEC_LIB) Makefile' >> .pjsua-$(TARGET_NAME).depend; \
	  echo '$(PJSIPPERF_EXE): $(PJSIP_LIB) $(PJSIP_UA_LIB) $(PJSIP_SIMPLE) $(PJSUA_LIB_LIB) $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJMEDIA_LIB) $(PJMEDIA_CODEC_LIB)' >> .pjsip-perf-$(TARGET_NAME).depend; \
	fi



