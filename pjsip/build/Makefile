include ../../build/common.mak

RULES_MAK := ../../build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

export PJSIP_LIB:=../lib/libpjsip-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
export PJSIP_UA_LIB:=../lib/libpjsip-ua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
export PJSIP_SIMPLE_LIB:=../lib/libpjsip-simple-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
export PJSUA_LIB_LIB=../lib/libpjsua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include \
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
# Defines for building PJSIP core library
#
export PJSIP_SRCDIR = ../src/pjsip
export PJSIP_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		sip_errno.o sip_msg.o sip_parser.o sip_tel_uri.o sip_uri.o \
		sip_endpoint.o sip_util.o sip_util_proxy.o \
		sip_resolve.o sip_transport.o sip_transport_loop.o sip_transport_udp.o \
		sip_auth_client.o sip_auth_msg.o sip_auth_parser.o sip_auth_server.o \
		sip_transaction.o sip_util_statefull.o \
		sip_dialog.o sip_ua_layer.o
export PJSIP_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building PJSIP UA library
#
export PJSIP_UA_SRCDIR = ../src/pjsip-ua
export PJSIP_UA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			sip_inv.o sip_reg.o sip_xfer.o
export PJSIP_UA_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJSIP SIMPLE library
#
export PJSIP_SIMPLE_SRCDIR = ../src/pjsip-simple
export PJSIP_SIMPLE_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			errno.o evsub.o evsub_msg.o pidf.o presence.o xpidf.o
export PJSIP_SIMPLE_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJSUA-LIB library
#
export PJSUA_LIB_SRCDIR = ../src/pjsua-lib
export PJSUA_LIB_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			pjsua_reg.o pjsua_call.o pjsua_pres.o pjsua_opt.o \
			pjsua_core.o
export PJSUA_LIB_CFLAGS += $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
#
TARGETS := pjsip pjsip-ua pjsip-simple pjsua-lib

.PHONY: $(TARGETS)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend pjsip pjsip-ua pjsua-lib clean realclean distclean

pjsip:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $(PJSIP_LIB)

pjsip-ua:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $(PJSIP_UA_LIB)

pjsip-simple:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $(PJSIP_SIMPLE_LIB)

pjsua-lib: 
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $(PJSUA_LIB_LIB)

.PHONY: ../lib/pjsip.ko
../lib/pjsip.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@

.PHONY: ../lib/pjsip-ua.ko
../lib/pjsip-ua.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@

.PHONY: ../lib/pjsip-simple.ko
../lib/pjsip-simple.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@

.PHONY: ../lib/pjsua-lib.ko
../lib/pjsua-lib.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@

clean depend realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@



