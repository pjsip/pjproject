# For common OSes, test's main() is defined in main.c.
# OS specific configuration may want to put it in different file.
# For example, see os-rtems.mak in current directory.
export TEST_OBJS = main.o

include ../../build.mak
include ../../version.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ../lib
export BINDIR := ../bin

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=../../pjnath/lib/libpjnath-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)

export PJSIP_LIB:=libpjsip-$(TARGET_NAME)$(LIBEXT)
export PJSIP_UA_LIB:=libpjsip-ua-$(TARGET_NAME)$(LIBEXT)
export PJSIP_SIMPLE_LIB:=libpjsip-simple-$(TARGET_NAME)$(LIBEXT)
export PJSUA_LIB_LIB:=libpjsua-$(TARGET_NAME)$(LIBEXT)
export PJSUA2_LIB_LIB=libpjsua2-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export PJSIP_SONAME := libpjsip.$(SHLIB_SUFFIX)
export PJSIP_SHLIB := $(PJSIP_SONAME).$(PJ_VERSION_MAJOR)
export PJSIP_UA_SONAME := libpjsip-ua.$(SHLIB_SUFFIX)
export PJSIP_UA_SHLIB := $(PJSIP_UA_SONAME).$(PJ_VERSION_MAJOR)
export PJSIP_SIMPLE_SONAME := libpjsip-simple.$(SHLIB_SUFFIX)
export PJSIP_SIMPLE_SHLIB := $(PJSIP_SIMPLE_SONAME).$(PJ_VERSION_MAJOR)
export PJSUA_LIB_SONAME := libpjsua.$(SHLIB_SUFFIX)
export PJSUA_LIB_SHLIB := $(PJSUA_LIB_SONAME).$(PJ_VERSION_MAJOR)
export PJSUA2_LIB_SONAME := libpjsua2.$(SHLIB_SUFFIX)
export PJSUA2_LIB_SHLIB := $(PJSUA2_LIB_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjnath/include \
		   $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) $(_CFLAGS)
export _LDFLAGS := $(APP_THIRD_PARTY_LIBS) \
		   $(APP_THIRD_PARTY_EXT) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS) 

###############################################################################
# Defines for building PJSIP core library
#
export PJSIP_SRCDIR = ../src/pjsip
export PJSIP_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		sip_config.o sip_multipart.o \
		sip_errno.o sip_msg.o sip_parser.o sip_tel_uri.o sip_uri.o \
		sip_endpoint.o sip_util.o sip_util_proxy.o \
		sip_resolve.o sip_transport.o sip_transport_loop.o \
		sip_transport_udp.o sip_transport_tcp.o \
		sip_transport_tls.o sip_auth_aka.o sip_auth_client.o \
		sip_auth_msg.o sip_auth_parser.o \
		sip_auth_server.o \
		sip_transaction.o sip_util_statefull.o \
		sip_dialog.o sip_ua_layer.o
export PJSIP_CFLAGS += $(_CFLAGS)
export PJSIP_CXXFLAGS += $(_CXXFLAGS)
export PJSIP_LDFLAGS += $(PJLIB_UTIL_LDLIB) \
			$(PJLIB_LDLIB) \
			$(_LDFLAGS)

###############################################################################
# Defines for building PJSIP UA library
#
export PJSIP_UA_SRCDIR = ../src/pjsip-ua
export PJSIP_UA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			sip_inv.o sip_reg.o sip_replaces.o sip_xfer.o \
			sip_100rel.o sip_timer.o sip_siprec.o
export PJSIP_UA_CFLAGS += $(_CFLAGS)
export PJSIP_UA_CXXFLAGS += $(_CXXFLAGS)
export PJSIP_UA_LDFLAGS += $(PJSIP_SIMPLE_LDLIB) \
			   $(PJSIP_LDLIB) \
			   $(PJMEDIA_LDLIB) \
			   $(PJLIB_UTIL_LDLIB) \
			   $(PJLIB_LDLIB) \
			   $(_LDFLAGS)


###############################################################################
# Defines for building PJSIP SIMPLE library
#
export PJSIP_SIMPLE_SRCDIR = ../src/pjsip-simple
export PJSIP_SIMPLE_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			errno.o evsub.o evsub_msg.o iscomposing.o \
			mwi.o pidf.o dialog_info.o presence.o dlg_event.o presence_body.o \
			publishc.o rpid.o xpidf.o
export PJSIP_SIMPLE_CFLAGS += $(_CFLAGS)
export PJSIP_SIMPLE_CXXFLAGS += $(_CXXFLAGS)
export PJSIP_SIMPLE_LDFLAGS += $(PJSIP_LDLIB) \
			       $(PJLIB_UTIL_LDLIB) \
			       $(PJLIB_LDLIB) \
			       $(_LDFLAGS)


###############################################################################
# Defines for building PJSUA-LIB library
#
export PJSUA_LIB_SRCDIR = ../src/pjsua-lib
export PJSUA_LIB_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			pjsua_acc.o pjsua_call.o pjsua_core.o \
			pjsua_im.o pjsua_media.o pjsua_pres.o \
			pjsua_dump.o pjsua_aud.o pjsua_txt.o pjsua_vid.o
export PJSUA_LIB_CFLAGS += $(_CFLAGS) $(PJ_VIDEO_CFLAGS)
export PJSUA_LIB_CXXFLAGS += $(_CXXFLAGS) $(PJ_VIDEO_CFLAGS)
export PJSUA_LIB_LDFLAGS += $(PJSIP_UA_LDLIB) \
			    $(PJSIP_SIMPLE_LDLIB) \
			    $(PJSIP_LDLIB) \
			    $(PJMEDIA_AUDIODEV_LDLIB) \
			    $(PJMEDIA_VIDEODEV_LDLIB) \
			    $(PJMEDIA_CODEC_LDLIB) \
			    $(PJMEDIA_LDLIB) \
			    $(PJNATH_LDLIB) \
			    $(PJLIB_UTIL_LDLIB) \
			    $(PJLIB_LDLIB) \
			    $(_LDFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 

###############################################################################
# Defines for building pjsua2 library
#
export PJSUA2_LIB_SRCDIR = ../src/pjsua2
export PJSUA2_LIB_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			   account.o endpoint.o json.o persistent.o types.o \
			   siptypes.o call.o presence.o media.o
export PJSUA2_LIB_CFLAGS += $(_CFLAGS) $(PJ_VIDEO_CFLAGS)
export PJSUA2_LIB_CXXFLAGS = $(_CXXFLAGS) $(PJSUA2_LIB_CFLAGS)
export PJSUA2_LIB_LDFLAGS += $(PJSUA_LIB_LDLIB) \
			     $(PJSIP_UA_LDLIB) \
			     $(PJSIP_SIMPLE_LDLIB) \
			     $(PJSIP_LDLIB) \
			     $(PJMEDIA_AUDIODEV_LDLIB) \
			     $(PJMEDIA_VIDEODEV_LDLIB) \
			     $(PJMEDIA_CODEC_LDLIB) \
			     $(PJMEDIA_LDLIB) \
			     $(PJNATH_LDLIB) \
			     $(PJLIB_UTIL_LDLIB) \
			     $(PJLIB_LDLIB) \
			     $(_LDFLAGS) \
			     -lstdc++


###############################################################################
# Defines for building pjsua2-test application
#
export PJSUA2_TEST_SRCDIR = ../src/pjsua2-test
export PJSUA2_TEST_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			   main.o
export PJSUA2_TEST_CFLAGS += $(_CFLAGS) $(PJ_VIDEO_CFLAGS)
export PJSUA2_TEST_CXXFLAGS = $(_CXXFLAGS) $(PJSUA2_LIB_CFLAGS) $(PJ_VIDEO_CFLAGS)
export PJSUA2_TEST_LDFLAGS += $(PJ_LDXXFLAGS) $(PJ_LDXXLIBS) $(LDFLAGS)
ifeq ($(EXCLUDE_APP),0)
export PJSUA2_TEST_EXE := pjsua2-test-$(TARGET_NAME)$(HOST_EXE)
endif
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 


###############################################################################
# Defines for building test application
#
export TEST_SRCDIR = ../src/test
export TEST_OBJS += dlg_core_test.o dns_test.o msg_err_test.o \
		    msg_logger.o msg_test.o multipart_test.o regc_test.o \
		    test.o transport_loop_test.o transport_tcp_test.o \
		    transport_test.o transport_udp_test.o \
		    tsx_basic_test.o tsx_bench.o tsx_uac_test.o \
		    tsx_uas_test.o txdata_test.o uri_test.o \
		    inv_offer_answer_test.o
export TEST_CFLAGS += $(_CFLAGS) $(PJ_VIDEO_CFLAGS)
export TEST_CXXFLAGS += $(_CXXFLAGS)
export TEST_LDFLAGS += $(PJSIP_LDLIB) \
		       $(PJSIP_UA_LDLIB) \
		       $(PJSIP_SIMPLE_LDLIB) \
		       $(PJSUA_LDLIB) \
		       $(PJMEDIA_CODEC_LDLIB) \
		       $(PJMEDIA_VIDEODEV_LDLIB) \
		       $(PJMEDIA_LDLIB) \
		       $(PJMEDIA_AUDIODEV_LDLIB) \
		       $(PJNATH_LDLIB) \
		       $(PJLIB_UTIL_LDLIB) \
		       $(PJLIB_LDLIB) \
		       $(_LDFLAGS)
ifeq ($(EXCLUDE_APP),0)
export TEST_EXE := pjsip-test-$(TARGET_NAME)$(HOST_EXE)
endif
	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
#
TARGETS := $(PJSIP_LIB) $(PJSIP_SONAME) \
	   $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME) \
	   $(PJSIP_UA_LIB) $(PJSIP_UA_SONAME) \
	   $(PJSUA_LIB_LIB) $(PJSUA_LIB_SONAME)
TARGETS_EXE := $(TEST_EXE)

ifeq ($(PJ_EXCLUDE_PJSUA2),)
TARGETS += $(PJSUA2_LIB_LIB) $(PJSUA2_LIB_SONAME)
TARGETS_EXE += $(PJSUA2_TEST_EXE)
endif

all: $(TARGETS) $(TARGETS_EXE)

lib: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjsip/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(PJSIP_LIB) $(PJSIP_SONAME)
.PHONY: $(PJSIP_UA_LIB) $(PJSIP_UA_SONAME)
.PHONY: $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME)
.PHONY: $(PJSUA_LIB_LIB) $(PJSUA_LIB_SONAME)
.PHONY: $(PJSUA2_LIB_LIB) $(PJSUA2_LIB_SONAME)
.PHONY: $(TEST_EXE)
.PHONY: $(PJSUA2_TEST_EXE)

pjsip: $(PJSIP_LIB)
$(PJSIP_SONAME): $(PJSIP_LIB)
$(PJSIP_LIB) $(PJSIP_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjsip-ua: $(PJSIP_UA_LIB)
$(PJSIP_UA_SONAME): $(PJSIP_UA_LIB)
$(PJSIP_UA_LIB) $(PJSIP_UA_SONAME): $(PJSIP_LIB) $(PJSIP_SONAME) $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjsip-simple: $(PJSIP_SIMPLE_LIB)
$(PJSIP_SIMPLE_SONAME): $(PJSIP_SIMPLE_LIB)
$(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME): $(PJSIP_LIB) $(PJSIP_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjsua-lib: $(PJSUA_LIB_LIB)
$(PJSUA_LIB_SONAME): $(PJSUA_LIB_LIB)
$(PJSUA_LIB_LIB) $(PJSUA_LIB_SONAME): $(PJSIP_LIB) $(PJSIP_SONAME) $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME) $(PJSIP_UA_LIB) $(PJSIP_UA_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjsua2-lib: $(PJSUA2_LIB_LIB)
$(PJSUA2_LIB_SONAME): $(PJSUA2_LIB_LIB)
$(PJSUA2_LIB_LIB) $(PJSUA2_LIB_SONAME): $(PJSUA_LIB_LIB) $(PJSUA_LIB_SONAME) $(PJSIP_LIB) $(PJSIP_SONAME) $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME) $(PJSIP_UA_LIB) $(PJSIP_UA_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_LIB app=pjsua2-lib $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjsip-test: $(TEST_EXE)
$(TEST_EXE): $(PJSIP_LIB) $(PJSIP_SONAME)
$(TEST_EXE): $(PJSIP_UA_LIB) $(PJSIP_UA_SONAME)
$(TEST_EXE): $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME)
$(TEST_EXE): $(PJSUA_LIB_LIB) $(PJSUA_LIB_SONAME)
$(TEST_EXE):
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

pjsua2-test: $(PJSUA2_TEST_EXE)
$(PJSUA2_TEST_EXE): $(PJSIP_LIB) $(PJSIP_SONAME)
$(PJSUA2_TEST_EXE): $(PJSIP_UA_LIB) $(PJSIP_UA_SONAME)
$(PJSUA2_TEST_EXE): $(PJSIP_SIMPLE_LIB) $(PJSIP_SIMPLE_SONAME)
$(PJSUA2_TEST_EXE): $(PJSUA_LIB_LIB) $(PJSUA_LIB_SONAME)
$(PJSUA2_TEST_EXE): $(PJSUA2_LIB_LIB) $(PJSUA2_LIB_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_TEST app=pjsua2-test $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

.PHONY: pjsip.ko
pjsip.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

.PHONY: pjsip-ua.ko
pjsip-ua.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

.PHONY: pjsip-simple.ko
pjsip-simple.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

.PHONY: pjsua-lib.ko
pjsua-lib.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

.PHONY: pjsua2-lib.ko
pjsua2-lib.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_LIB app=pjsua2-lib $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_LIB app=pjsua2-lib $@
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_TEST app=pjsua2-test $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_LIB app=pjsua2-lib $@
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_TEST app=pjsua2-test $@
	echo '$(BINDIR)/$(TEST_EXE): $(PJMEDIA_LIB) $(LIBDIR)/$(PJSUA_LIB_LIB) $(LIBDIR)/$(PJSIP_SIMPLE_LIB) $(LIBDIR)/$(PJSIP_UA_LIB) $(LIBDIR)/$(PJSIP_LIB) $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjsip-test-$(TARGET_NAME).depend
	echo '$(BINDIR)/$(PJSUA2_TEST_EXE): $(LIBDIR)/$(PJSUA2_LIB_LIB) $(PJMEDIA_LIB) $(LIBDIR)/$(PJSUA_LIB_LIB) $(LIBDIR)/$(PJSIP_SIMPLE_LIB) $(LIBDIR)/$(PJSIP_UA_LIB) $(LIBDIR)/$(PJSIP_LIB) $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjsua2-test-$(TARGET_NAME).depend

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsip-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsip-ua-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsip-simple-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsua-lib-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsua2-lib-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsua2-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_LIB app=pjsua2-lib $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA2_TEST app=pjsua2-test $@



