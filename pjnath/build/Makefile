# By default, the test application includes main.o.
# OS make file may override this with os-specific files
export PJNATH_TEST_OBJS = main.o

include ../../build.mak
include ../../version.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ../lib
export BINDIR := ../bin

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)

export PJNATH_LIB:=libpjnath-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export PJNATH_SONAME := libpjnath.$(SHLIB_SUFFIX)
export PJNATH_SHLIB := $(PJNATH_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS) 

###############################################################################
# Defines for building PJNATH library
#
export PJNATH_SRCDIR = ../src/pjnath
export PJNATH_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		errno.o ice_session.o ice_strans.o nat_detect.o stun_auth.o \
		stun_msg.o stun_msg_dump.o stun_session.o stun_sock.o \
		stun_transaction.o turn_session.o turn_sock.o
export PJNATH_CFLAGS += $(_CFLAGS)
export PJNATH_CXXFLAGS += $(_CXXFLAGS)
export PJNATH_LDFLAGS += $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB) $(_LDFLAGS)

###############################################################################
# Defines for building test application
#
export PJNATH_TEST_SRCDIR = ../src/pjnath-test
export PJNATH_TEST_OBJS += ice_test.o stun.o sess_auth.o server.o concur_test.o \
			    stun_sock_test.o turn_sock_test.o test.o
export PJNATH_TEST_CFLAGS += $(_CFLAGS)
export PJNATH_TEST_CXXFLAGS += $(_CXXFLAGS)
export PJNATH_TEST_LDFLAGS += $(PJNATH_LDLIB) $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB) $(_LDFLAGS)
ifeq ($(EXCLUDE_APP),0)
export PJNATH_TEST_EXE:=pjnath-test-$(TARGET_NAME)$(HOST_EXE)
endif
	
###############################################################################
# Defines for building TURN client application
#
export PJTURN_CLIENT_SRCDIR = ../src/pjturn-client
export PJTURN_CLIENT_OBJS += client_main.o
export PJTURN_CLIENT_CFLAGS += $(_CFLAGS)
export PJTURN_CLIENT_CXXFLAGS += $(_CXXFLAGS)
export PJTURN_CLIENT_LDFLAGS += $(PJNATH_LDLIB) $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB) $(_LDFLAGS)
ifeq ($(EXCLUDE_APP),0)
export PJTURN_CLIENT_EXE:=pjturn-client-$(TARGET_NAME)$(HOST_EXE)
endif

###############################################################################
# Defines for building TURN server application
#
export PJTURN_SRV_SRCDIR = ../src/pjturn-srv
export PJTURN_SRV_OBJS += allocation.o auth.o listener_udp.o \
			  listener_tcp.o server.o main.o
export PJTURN_SRV_CFLAGS += $(_CFLAGS)
export PJTURN_SRV_CXXFLAGS += $(_CXXFLAGS)
export PJTURN_SRV_LDFLAGS += $(PJNATH_LDLIB) $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB) $(_LDFLAGS)
ifeq ($(EXCLUDE_APP),0)
export PJTURN_SRV_EXE:=pjturn-srv-$(TARGET_NAME)$(HOST_EXE)
endif
	
	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
TARGETS := $(PJNATH_LIB) $(PJNATH_SONAME)
TARGETS_EXE := $(PJNATH_TEST_EXE) $(PJTURN_CLIENT_EXE) $(PJTURN_SRV_EXE)

all: $(TARGETS) $(TARGETS_EXE)

lib: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjnath/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(PJNATH_LIB) $(PJNATH_SONAME)
.PHONY: $(PJNATH_TEST_EXE) $(PJTURN_CLIENT_EXE) $(PJTURN_SRV_EXE)

pjnath: $(PJNATH_LIB)
$(PJNATH_SONAME): $(PJNATH_LIB)
$(PJNATH_LIB) $(PJNATH_SONAME): $(PJLIB_LIB) $(PJLIB_SONAME) $(PJLIB_UTIL_LIB) $(PJLIB_UTIL_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjnath-test: $(PJNATH_TEST_EXE)
$(PJNATH_TEST_EXE): $(PJNATH_LIB) $(PJNATH_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

pjturn-client: $(PJTURN_CLIENT_EXE)
$(PJTURN_CLIENT_EXE): $(PJNATH_LIB) $(PJNATH_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

pjturn-srv: $(PJTURN_SRV_EXE)
$(PJTURN_SRV_EXE): $(PJNATH_LIB) $(PJNATH_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

.PHONY: pjnath.ko
pjnath.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

.PHONY: pjnath-test.ko
pjnath-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjnath-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjnath-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjturn-client-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjturn-srv-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $@
	echo '$(BINDIR)/$(PJNATH_TEST_EXE): $(LIBDIR)/$(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjnath-test-$(TARGET_NAME).depend
	echo '$(BINDIR)/$(PJTURN_CLIENT_EXE): $(LIBDIR)/$(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjturn-client-$(TARGET_NAME).depend
	echo '$(BINDIR)/$(PJTURN_SRV_EXE): $(LIBDIR)/$(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjturn-srv-$(TARGET_NAME).depend


