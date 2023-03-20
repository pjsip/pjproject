# Read Fuzzing.md located at the root of pjproject dir for build instructions

#Modify this to point to the PJSIP location.
PJBASE=../../

include $(PJBASE)/build.mak

TARGET=Fuzzing

JSON=fuzz-json
XML=fuzz-xml
SDP=fuzz-sdp
STUN=fuzz-stun
SIP=fuzz-sip
RTCP=fuzz-rtcp
DNS=fuzz-dns
H264=fuzz-h264
VPX=fuzz-vpx
HTTP=fuzz-http
URL=fuzz-url
URI=fuzz-uri
CRYPTO=fuzz-crypto

EXTFLAGS=-Wall -Werror

all: $(TARGET)

$(TARGET):
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(JSON).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(XML).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(SDP).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(STUN).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(SIP).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(RTCP).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(DNS).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(H264).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(VPX).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(HTTP).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(URL).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(URI).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(CRYPTO).c

	$(CXX) $(PJ_CFLAGS) -o $(JSON) $(JSON).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(XML)  $(XML).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(SDP)  $(SDP).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(STUN) $(STUN).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(SIP)  $(SIP).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(RTCP) $(RTCP).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(DNS)  $(DNS).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(H264) $(H264).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(VPX)  $(VPX).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(HTTP) $(HTTP).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(URL)  $(URL).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(URI)  $(URI).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(CRYPTO) $(CRYPTO).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE) -lssl -lcrypto -lz

clean:
	rm $(JSON) $(XML) $(SDP) $(STUN) $(SIP) $(RTCP) $(DNS) $(H264) $(VPX) $(HTTP) $(URL) $(URI) $(CRYPTO) *.o
