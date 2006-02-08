include ../../build/common.mak

RULES_MAK := ../../build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

export PJSIP_LIB:=../lib/libpjsip-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
export PJSIP_UA_LIB:=../lib/libpjsip-ua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJSIP_UA_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJSIP_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 


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
			sip_inv.o sip_reg.o
export PJSIP_UA_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJSUA
#
export PJSUA_SRCDIR = ../src/pjsua
export PJSUA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			main.o pjsua_reg.o pjsua_core.o
export PJSUA_CFLAGS += $(_CFLAGS)
export PJSUA_LDFLAGS += $(_LDFLAGS)
export PJSUA_EXE:=../bin/pjsua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE)



export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
#
TARGETS := pjsip pjsip-ua pjsua

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend pjsip pjsip-ua pjsua clean realclean distclean

pjsip:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $(PJSIP_LIB)

pjsip-ua:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $(PJSIP_UA_LIB)

pjsua: $(PJSIP_LIB) $(PJSIP_UA_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $(PJSUA_EXE)

.PHONY: ../lib/pjsip.ko
../lib/pjsip.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@

.PHONY: ../lib/pjsip-ua.ko
../lib/pjsip-ua.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@

.PHONY: ../lib/pjsua.ko
../lib/pjsua.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $@

clean depend realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $@
	@if test "$@" == "depend"; then \
	  echo '$(PJSUA_EXE): $(PJSIP_LIB) $(PJSIP_UA_LIB)' >> .pjsua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).depend; \
	fi



