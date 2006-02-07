include ../../build/common.mak

RULES_MAK := ../../build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
export PJLIB_UTIL_LIB:=../lib/libpjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= -O2 -g $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PJLIB-UTIL library
#
export PJLIB_UTIL_SRCDIR = ../src/pjlib-util
export PJLIB_UTIL_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		md5.o scanner.o stun.o string.o stun.o stun_client.o xml.o
export PJLIB_UTIL_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building test application
#
export UTIL_TEST_SRCDIR = ../src/pjlib-util-test
export UTIL_TEST_OBJS += xml.o test.o main.o
export UTIL_TEST_CFLAGS += $(_CFLAGS)
export UTIL_TEST_LDFLAGS += $(_LDFLAGS)
export UTIL_TEST_EXE:=../bin/pjlib-util-test-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := pjlib-util pjlib-util-test

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend pjlib pjlib-test clean realclean distclean

pjlib-util:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $(PJLIB_UTIL_LIB)

pjlib-util-test: $(PJLIB_LIB)
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $(UTIL_TEST_EXE)

.PHONY: ../lib/pjlib-util.ko
../lib/pjlib-util.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $@

.PHONY: ../lib/pjlib-util-test.ko
../lib/pjlib-util-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $@

clean depend realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB_UTIL app=pjlib-util $@
	$(MAKE) -f $(RULES_MAK) APP=UTIL_TEST app=pjlib-util-test $@
	@if test "$@" == "depend"; then \
	  echo '$(TEST_EXE): $(PJLIB_UTIL_LIB)' >> .pjlib-util-test-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).depend; \
	fi


