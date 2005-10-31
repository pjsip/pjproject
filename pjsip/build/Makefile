include make-$(TARGET).inc

export PJSIP_SRCDIR = ../src/pjsip
export PJSIP_SRCEXT = .c
export PJSIP_SRCS = $(PJSIP_SOURCES) sip_auth.c sip_auth_msg.c sip_auth_parser.c \
		    sip_endpoint.c sip_misc.c sip_msg.c sip_parser.c \
		    sip_resolve.c sip_transaction.c sip_transport.c sip_uri.c

export PJSIP_UA_SRCDIR = ../src/pjsip_mod_ua
export PJSIP_UA_SRCEXT = .c
export PJSIP_UA_SRCS = $(PJSIP_UA_SOURCES) sip_dialog.c sip_reg.c sip_ua.c 

export PJSIP_SIMPLE_SRCDIR = ../src/pjsip_simple
export PJSIP_SIMPLE_SRCEXT = .c
export PJSIP_SIMPLE_SRCS = $(PJSIP_SIMPLE_SOURCES) event_notify.c event_notify_msg.c \
		    messaging.c pidf.c presence.c xpidf.c

export PJSUA_SRCDIR = ../src/pjsua
export PJSUA_SRCEXT = .c
export PJSUA_SRCS = $(PJSUA_SOURCES) main.c getopt.c

export TARGET CCOUT CC AR RANLIB MV RM RMDIR MKDIR OBJEXT LD LDOUT 

all: pjsip pjsip_ua pjsip_simple pjsua 

doc:
	cd .. && doxygen docs/doxygen.cfg

print:
	$(MAKE) -f make-rules APP=PJSIP app=pjsip print_lib
	$(MAKE) -f make-rules APP=PJSIP_UA app=pjsip_ua print_lib
	$(MAKE) -f make-rules APP=PJSIP_SIMPLE app=pjsip_simple print_lib
	$(MAKE) -f make-rules APP=PJSUA app=pjsua print_bin
	
depend:
	$(MAKE) -f make-rules APP=PJSIP app=pjsip depend
	$(MAKE) -f make-rules APP=PJSUA app=pjsua depend
	$(MAKE) -f make-rules APP=PJSIP_UA app=pjsip_ua depend
	$(MAKE) -f make-rules APP=PJSIP_SIMPLE app=pjsip_simple depend
	echo '$(PJSUA_EXE): $(PJSIP_LIB) $(PJSIP_UA_LIB)' >> .pjsua.depend

dep: depend

pjsip:
	$(MAKE) -f make-rules APP=PJSIP app=pjsip $(PJSIP_LIB)

pjsua: 
	$(MAKE) -f make-rules APP=PJSUA app=pjsua $(PJSUA_EXE)

pjsip_ua:
	$(MAKE) -f make-rules APP=PJSIP_UA app=pjsip_ua $(PJSIP_UA_LIB)

pjsip_simple:
	$(MAKE) -f make-rules APP=PJSIP_SIMPLE app=pjsip_simple $(PJSIP_SIMPLE_LIB)

samples:
	gcc $(_CFLAGS) -o ../bin/simpleua ../src/samples/simpleua.c $(_LDFLAGS) 

clean:
	$(MAKE) -f make-rules APP=PJSIP app=pjsip clean
	$(MAKE) -f make-rules APP=PJSUA app=pjsua clean
	$(MAKE) -f make-rules APP=PJSIP_UA app=pjsip_ua clean
	$(MAKE) -f make-rules APP=PJSIP_SIMPLE app=pjsip_simple clean

realclean:
	$(MAKE) -f make-rules APP=PJSIP app=pjsip realclean
	$(MAKE) -f make-rules APP=PJSUA app=pjsua realclean
	$(MAKE) -f make-rules APP=PJSIP_UA app=pjsip_ua realclean
	$(MAKE) -f make-rules APP=PJSIP_SIMPLE app=pjsip_simple realclean

distclean: realclean


