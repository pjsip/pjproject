# By default, the test application includes main.o.
# OS make file may override this with os-specific files
export PJNATH_TEST_OBJS = main.o

include ../../build.mak
include $(PJDIR)/build/common.mak

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
export PJNATH_LIB:=../lib/libpjnath-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJNATH_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PJNATH library
#
export PJNATH_SRCDIR = ../src/pjnath
export PJNATH_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		errno.o ice_session.o ice_strans.o nat_detect.o stun_auth.o \
		stun_msg.o stun_msg_dump.o stun_session.o stun_transaction.o
export PJNATH_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building test application
#
export PJNATH_TEST_SRCDIR = ../src/pjnath-test
export PJNATH_TEST_OBJS += ice_test.o stun.o test.o
export PJNATH_TEST_CFLAGS += $(_CFLAGS)
export PJNATH_TEST_LDFLAGS += $(_LDFLAGS)
export PJNATH_TEST_EXE:=../bin/pjnath-test-$(TARGET_NAME)$(HOST_EXE)

	
###############################################################################
# Defines for building test client application
#
export PJSTUN_CLIENT_SRCDIR = ../src/pjstun-client
export PJSTUN_CLIENT_OBJS += client_main.o
export PJSTUN_CLIENT_CFLAGS += $(_CFLAGS)
export PJSTUN_CLIENT_LDFLAGS += $(_LDFLAGS)
export PJSTUN_CLIENT_EXE:=../bin/pjstun-client-$(TARGET_NAME)$(HOST_EXE)

###############################################################################
# Defines for building test server application
#
export PJSTUN_SRV_TEST_SRCDIR = ../src/pjstun-srv-test
export PJSTUN_SRV_TEST_OBJS += 	bind_usage.o server.o turn_usage.o usage.o \
				main.o
export PJSTUN_SRV_TEST_CFLAGS += $(_CFLAGS)
export PJSTUN_SRV_TEST_LDFLAGS += $(_LDFLAGS)
export PJSTUN_SRV_TEST_EXE:=../bin/pjstun-srv-test-$(TARGET_NAME)$(HOST_EXE)

	
	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := pjnath pjnath-test pjstun-client pjstun-srv-test

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend pjlib pjlib-test clean realclean distclean

pjnath:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $(PJNATH_LIB)

pjnath-test: $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJNATH_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $(PJNATH_TEST_EXE)

pjstun-client: $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJNATH_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_CLIENT app=pjstun-client $(PJSTUN_CLIENT_EXE)

pjstun-srv-test: $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJNATH_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_SRV_TEST app=pjstun-srv-test $(PJSTUN_SRV_TEST_EXE)

.PHONY: ../lib/pjnath.ko
../lib/pjnath.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@

.PHONY: ../lib/pjnath-test.ko
../lib/pjnath-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_CLIENT app=pjstun-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_SRV_TEST app=pjstun-srv-test $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjnath-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjnath-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjstun-client-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjstun-srv-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_CLIENT app=pjstun-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_SRV_TEST app=pjstun-srv-test $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_CLIENT app=pjstun-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJSTUN_SRV_TEST app=pjstun-srv-test $@
	echo '$(TEST_EXE): $(PJNATH_LIB)' >> .pjnath-test-$(TARGET_NAME).depend; \


