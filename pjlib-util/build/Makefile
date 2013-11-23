# By default, the test application includes main.o.
# OS make file may override this with os-specific files
export UTIL_TEST_OBJS = main.o

include ../../build.mak
include ../../version.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ../lib
export BINDIR := ../bin

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=$(PJDIR)/pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)

export PJLIB_UTIL_LIB := libpjlib-util-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export PJLIB_UTIL_SONAME := libpjlib-util.$(SHLIB_SUFFIX)
export PJLIB_UTIL_SHLIB := $(PJLIB_UTIL_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS)

###############################################################################
# Defines for building PJLIB-UTIL library
#
export PJLIB_UTIL_SRCDIR = ../src/pjlib-util
export PJLIB_UTIL_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		base64.o cli.o cli_console.o cli_telnet.o crc32.o errno.o dns.o \
		dns_dump.o dns_server.o getopt.o hmac_md5.o hmac_sha1.o \
		http_client.o md5.o pcap.o resolver.o scanner.o sha1.o \
		srv_resolver.o string.o stun_simple.o \
		stun_simple_client.o xml.o
export PJLIB_UTIL_CFLAGS += $(_CFLAGS)
export PJLIB_UTIL_CXXFLAGS += $(_CXXFLAGS)
export PJLIB_UTIL_LDFLAGS += $(PJLIB_LDLIB) $(_LDFLAGS)

###############################################################################
# Defines for building test application
#
export UTIL_TEST_SRCDIR = ../src/pjlib-util-test
export UTIL_TEST_OBJS += xml.o encryption.o stun.o resolver_test.o test.o \
		http_client.o
export UTIL_TEST_CFLAGS += $(_CFLAGS)
export UTIL_TEST_CXXFLAGS += $(_CXXFLAGS)
export UTIL_TEST_LDFLAGS += $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB) $(_LDFLAGS)
export UTIL_TEST_EXE:=pjlib-util-test-$(TARGET_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(PJLIB_UTIL_LIB) $(PJLIB_UTIL_SONAME) $(UTIL_TEST_EXE)

all: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjlib-util/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjlib-util/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjlib-util/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjlib-util/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjlib-util/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjlib-util/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(PJLIB_UTIL_LIB) $(PJLIB_UTIL_SONAME)
.PHONY: $(UTIL_TEST_EXE)

pjlib-util: $(PJLIB_UTIL_LIB) $(PJLIB_UTIL_SONAME)
$(PJLIB_UTIL_SONAME): $(PJLIB_UTIL_LIB)
$(PJLIB_UTIL_LIB) $(PJLIB_UTIL_SONAME): $(PJLIB_LIB) $(PJLIB_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjlib-util-test: $(UTIL_TEST_EXE)
$(UTIL_TEST_EXE): $(PJLIB_UTIL_LIB) $(PJLIB_UTIL_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

.PHONY: pjlib-util.ko
pjlib-util.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

.PHONY: pjlib-util-test.ko
pjlib-util-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $@
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjlib-util-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjlib-util-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $@
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $@
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $@
	echo '$(BINDIR)/$(UTIL_TEST_EXE): $(LIBDIR)/$(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjlib-util-test-$(TARGET_NAME).depend; \

