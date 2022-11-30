
#Fuzzing 
export DIR=cd tests/fuzz/ &&
export EXTFLAGS=-Wall -Werror

export json=fuzz-json
export xml=fuzz-xml
export sdp=fuzz-sdp
export stun=fuzz-stun
export sip=fuzz-sip

export FUZZ_pjlib=$(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB)
export FUZZ_pjmedia=$(PJMEDIA_CODEC_LDLIB) $(PJMEDIA_VIDEODEV_LDLIB) $(PJMEDIA_AUDIODEV_LDLIB) $(PJMEDIA_LDLIB) $(PJNATH_LDLIB) $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB)
export FUZZ_pjnath=$(PJNATH_LDLIB) $(PJLIB_UTIL_LDLIB) $(PJLIB_LDLIB)
export FUZZ_pjsip=$(PJSIP_LDLIB) $(PJSIP_UA_LDLIB) $(PJSIP_SIMPLE_LDLIB)  $(PJMEDIA_CODEC_LDLIB) $(PJMEDIA_VIDEODEV_LDLIB) $(PJMEDIA_LDLIB) $(PJMEDIA_AUDIODEV_LDLIB) $(PJLIB_UTIL_LDLIB) $(PJNATH_LDLIB) $(PJLIB_LDLIB)



	$(DIR) $(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(json).c
	$(DIR) $(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(xml).c
	$(DIR) $(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(sdp).c
	$(DIR) $(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(stun).c
	$(DIR) $(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(sip).c

	$(DIR) $(CXX) $(PJ_CFLAGS) -o $(json) $(json).o $(PJ_LDFLAGS) $(LIB_FUZZING_ENGINE) $(FUZZ_pjlib)
	$(DIR) $(CXX) $(PJ_CFLAGS) -o $(xml) $(xml).o $(PJ_LDFLAGS) $(LIB_FUZZING_ENGINE) $(FUZZ_pjlib)
	$(DIR) $(CXX) $(PJ_CFLAGS) -o $(sdp) $(sdp).o $(PJ_LDFLAGS) $(LIB_FUZZING_ENGINE) $(FUZZ_pjmedia)
	$(DIR) $(CXX) $(PJ_CFLAGS) -o $(stun) $(stun).o $(PJ_LDFLAGS) $(LIB_FUZZING_ENGINE) $(FUZZ_pjnath)
	$(DIR) $(CXX) $(PJ_CFLAGS) -o $(sip) $(sip).o $(PJ_LDFLAGS) $(LIB_FUZZING_ENGINE) $(FUZZ_pjsip)