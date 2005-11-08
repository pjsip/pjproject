#
# Include host/target/compiler selection.
# This will export CC_NAME, MACHINE_NAME, OS_NAME, and HOST_NAME variables.
#
include ../../build.mak

#
# Include global compiler specific definitions
#
include ../../build/cc-$(CC_NAME).mak

#
# (Optionally) Include compiler specific configuration that is
# specific to this project. This configuration file is
# located in this directory.
#
-include cc-$(CC_NAME).mak

#
# Include global machine specific definitions
#
include ../../build/m-$(MACHINE_NAME).mak
-include m-$(MACHINE_NAME).mak

#
# Include target OS specific definitions
#
include ../../build/os-$(OS_NAME).mak

#
# (Optionally) Include target OS specific configuration that is
# specific to this project. This configuration file is
# located in this directory.
#
-include os-$(OS_NAME).mak

#
# Include host specific definitions
#
include ../../build/host-$(HOST_NAME).mak

#
# (Optionally) Include host specific configuration that is
# specific to this project. This configuration file is
# located in this directory.
#
-include host-$(HOST_NAME).mak

#
# Include global user configuration, if any
#
-include ../../user.mak


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
# Defines for building PJLIB library
#
export PJLIB_UTIL_SRCDIR = ../src/pjlib-util
export PJLIB_UTIL_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		md5.o scanner.o stun.o stun_client.o xml.o
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


