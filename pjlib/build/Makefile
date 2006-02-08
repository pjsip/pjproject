include ../../build/common.mak

RULES_MAK := ../../build/rules.mak


export PJLIB_LIB := ../lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PJLIB library
#
export PJLIB_SRCDIR = ../src/pj
export PJLIB_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
	array.o config.o ctype.o errno.o except.o fifobuf.o guid.o \
	hash.o list.o lock.o log.o \
	pool.o pool_caching.o rand.o \
	rbtree.o string.o timer.o \
	types.o symbols.o
export PJLIB_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building test application
#
export TEST_SRCDIR = ../src/pjlib-test
export TEST_OBJS += atomic.o echo_clt.o errno.o exception.o \
		    fifobuf.o file.o \
		    ioq_perf.o ioq_udp.o ioq_tcp.o \
		    list.o mutex.o os.o pool.o pool_perf.o rand.o rbtree.o \
		    select.o sleep.o sock.o sock_perf.o \
		    string.o test.o thread.o timer.o timestamp.o \
		    udp_echo_srv_sync.o udp_echo_srv_ioqueue.o \
		    util.o
export TEST_CFLAGS += $(_CFLAGS)
export TEST_LDFLAGS += $(_LDFLAGS)
export TEST_EXE := ../bin/pjlib-test-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

print:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib print_lib
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test print_bin
	
depend: ../include/pj/config_site.h
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib depend
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test depend
	echo '$(TEST_EXE): $(PJLIB_LIB)' >> .pjlib-test-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).depend


.PHONY: dep depend pjlib pjlib-test clean realclean distclean

dep: depend

pjlib: ../include/pj/config_site.h
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib $(PJLIB_LIB)

../include/pj/config_site.h:
	touch ../include/pj/config_site.h
	
pjlib-test: 
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test $(TEST_EXE)

.PHONY: ../lib/pjlib.ko
../lib/pjlib.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib $@

.PHONY: ../lib/pjlib-test.ko
../lib/pjlib-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test $@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib clean
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test clean

realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib realclean
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test realclean

distclean: realclean

gcov-report:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib gcov-report
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test gcov-report

