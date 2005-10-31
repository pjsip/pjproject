include make-$(TARGET).inc

export PJMEDIA_SRCEXT = .c
export PJMEDIA_SRCDIR = ../src/pjmedia
export PJMEDIA_SRCS = 	$(PJMEDIA_SOURCES) codec.c jbuf.c rtp.c mediamgr.c \
		      	session.c g711.c  rtcp.c  stream.c sdp.c pasound.c \
		      	$(PA_DIR)/pa_allocation.c \
			$(PA_DIR)/pa_converters.c \
			$(PA_DIR)/pa_cpuload.c \
			$(PA_DIR)/pa_dither.c \
			$(PA_DIR)/pa_front.c \
			$(PA_DIR)/pa_process.c \
			$(PA_DIR)/pa_skeleton.c \
			$(PA_DIR)/pa_stream.c \
			$(PA_DIR)/pa_trace.c

export TEST_SRCEXT = .c
export TEST_SRCDIR = ../src/test
export TEST_SRCS = $(TEST_SOURCES) jbuf_test.c rtp_test.c main.c \
		session_test.c sdptest.c
		   

export PJAUT_SRCEXT = .c
export PJAUT_SRCDIR = ../src/test
export PJAUT_SRCS = $(PJAUT_SOURCES) audio_tool.c

export CCOUT CC AR RANLIB MV RM RMDIR MKDIR OBJEXT LD LDOUT 

all: pjmedia test pjaut

doc:
	cd .. && doxygen docs/doxygen.cfg

print:
	$(MAKE) -f make-rules APP=PJMEDIA app=pjmedia print_lib
	$(MAKE) -f make-rules APP=TEST app=test print_bin
	$(MAKE) -f make-rules APP=PJAUT app=pjaut print_bin
	
depend:
	$(MAKE) -f make-rules APP=PJMEDIA app=pjmedia depend
	$(MAKE) -f make-rules APP=TEST app=test depend
	$(MAKE) -f make-rules APP=PJAUT app=pjaut depend

dep: depend

pjmedia:
	$(MAKE) -f make-rules APP=PJMEDIA app=pjmedia $(PJMEDIA_LIB)

test:
	$(MAKE) -f make-rules APP=TEST app=test $(TEST_EXE)

pjaut:
	$(MAKE) -f make-rules APP=PJAUT app=pjaut $(PJAUT_EXE)
	
clean:
	$(MAKE) -f make-rules APP=PJMEDIA app=pjmedia clean
	$(MAKE) -f make-rules APP=TEST app=test clean
	$(MAKE) -f make-rules APP=PJAUT app=pjaut clean

realclean:
	$(MAKE) -f make-rules APP=PJMEDIA app=pjmedia realclean
	$(MAKE) -f make-rules APP=TEST app=test realclean
	$(MAKE) -f make-rules APP=PJAUT app=pjaut realclean

distclean: realclean


