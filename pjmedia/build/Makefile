include ../../build/common.mak

RULES_MAK := ../../build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

export PJMEDIA_LIB:=../lib/libpjmedia-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../src/pjmedia/portaudio
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PJMEDIA library
#
PA_DIR:=portaudio
export PJMEDIA_SRCDIR = ../src/pjmedia
export PJMEDIA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			codec.o endpoint.o errno.o g711.o jbuf.o rtcp.o \
			rtp.o sdp.o sdp_cmp.o sdp_neg.o session.o stream.o \
			pasound.o \
		      	$(PA_DIR)/pa_allocation.o \
			$(PA_DIR)/pa_converters.o \
			$(PA_DIR)/pa_cpuload.o \
			$(PA_DIR)/pa_dither.o \
			$(PA_DIR)/pa_front.o \
			$(PA_DIR)/pa_process.o \
			$(PA_DIR)/pa_skeleton.o \
			$(PA_DIR)/pa_stream.o \
			$(PA_DIR)/pa_trace.o

export PJMEDIA_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building test application
#
export PJMEDIA_TEST_SRCDIR = ../src/test
export PJMEDIA_TEST_OBJS += jbuf_test.o main.o rtp_test.o sdp_neg_test.o test.o
export PJMEDIA_TEST_CFLAGS += $(_CFLAGS)
export PJMEDIA_TEST_LDFLAGS += $(_LDFLAGS)
export PJMEDIA_TEST_EXE:=../bin/pjmedia-test-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := pjmedia 
#pjmedia-test

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend pjmedia pjmedia-test clean realclean distclean

pjmedia:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $(PJMEDIA_LIB)

pjmedia-test: $(PJMEDIA_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $(PJMEDIA_TEST_EXE)

.PHONY: ../lib/pjmedia.ko
../lib/pjmedia.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@

.PHONY: ../lib/pjmedia-test.ko
../lib/pjmedia-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@

clean depend realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@
	@if test "$@" == "depend"; then \
	  echo '$(PJMEDIA_TEST_EXE): $(PJMEDIA_LIB)' >> .pjmedia-test-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).depend; \
	fi


