include make-$(TARGET).inc

export PJLIB_SRCDIR = ../src/pj
export PJLIB_SRCEXT = .c
export PJLIB_SRCS = $(PJLIB_SOURCES) array.c config.c except.c fifobuf.c guid.c \
	hash.c ioqueue_select.c list.c log.c log_stdout.c \
	pool.c pool_caching.c pool_policy.c \
	rbtree.c scanner.c sock.c string.c timer.c types.c md5.c stun.c stun_client.c \
	xml.c

export TEST_SRCDIR = ../src/test
export TEST_SRCEXT = .cpp
export TEST_SRCS = $(TEST_SOURCES) exception_test.cpp fifobuf_test.cpp \
		ioqueue_test.cpp ioqueue_test_tcp.cpp \
		ioqueue_test_udp.cpp list_test.cpp main.cpp \
		os_test.cpp pool_test.cpp rbtree_test.cpp \
		timer_test.cpp xml_test.cpp

export TARGET CCOUT CC AR RANLIB MV RM RMDIR MKDIR OBJEXT LD LDOUT 

all: pjlib test

doc:
	cd .. && doxygen docs/doxygen.cfg

print:
	$(MAKE) -f make-rules APP=PJLIB app=pjlib print_lib
	$(MAKE) -f make-rules APP=TEST app=test print_bin
	
depend:
	$(MAKE) -f make-rules APP=PJLIB app=pjlib depend
	$(MAKE) -f make-rules APP=TEST app=test depend
	echo '$(TEST_EXE): $(PJLIB_LIB)' >> .test.depend


dep: depend

pjlib:
	$(MAKE) -f make-rules APP=PJLIB app=pjlib $(PJLIB_LIB)


test: 
	$(MAKE) -f make-rules APP=TEST app=test $(TEST_EXE)


clean:
	$(MAKE) -f make-rules APP=PJLIB app=pjlib clean
	$(MAKE) -f make-rules APP=TEST app=test clean

realclean:
	$(MAKE) -f make-rules APP=PJLIB app=pjlib realclean
	$(MAKE) -f make-rules APP=TEST app=test realclean

distclean: realclean

