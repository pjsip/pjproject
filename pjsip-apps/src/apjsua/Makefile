include ../../../build.mak
include $(PJDIR)/build/common.mak

RULES_MAK := $(PJDIR)/build/rules.mak

SWIG=swig

all: pjsua_app_wrap.o
	${APP_CXX} ${APP_CXXFLAGS} ${APP_LDFLAGS} -o libs/armeabi/libpjsua_app.so swig/pjsua_app_wrap.o ../../build/output/pjsua-$(TARGET_NAME)/*.o ${APP_LDLIBS}

pjsua_app_wrap.o: pjsua_app.i
	${CC} swig/pjsua_app_wrap.c ${APP_CFLAGS} -o swig/pjsua_app_wrap.o

pjsua_app.i: swig/pjsua_app.i
	${SWIG} -package org.pjsip.pjsua -outdir src/org/pjsip/pjsua -java swig/pjsua_app.i
