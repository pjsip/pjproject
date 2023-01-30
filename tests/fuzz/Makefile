#Modify this to point to the PJSIP location.
PJBASE=../../

include $(PJBASE)/build.mak

TARGET=Fuzzing

JSON=fuzz-json
XML=fuzz-xml
SDP=fuzz-sdp
STUN=fuzz-stun
SIP=fuzz-sip

EXTFLAGS=-Wall -Werror

all: $(TARGET)

$(TARGET):
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(JSON).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(XML).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(SDP).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(STUN).c
	$(CC) $(PJ_CFLAGS) $(EXTFLAGS) -c $(SIP).c

	$(CXX) $(PJ_CFLAGS) -o $(JSON) $(JSON).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(XML)  $(XML).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(SDP)  $(SDP).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(STUN) $(STUN).o $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)
	$(CXX) $(PJ_CFLAGS) -o $(SIP)  $(SIP).o  $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) $(LIB_FUZZING_ENGINE)

clean:
	rm $(JSON) $(XML) $(SDP) $(STUN) $(SIP) *.o
