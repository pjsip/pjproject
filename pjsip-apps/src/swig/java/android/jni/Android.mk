include ../../../../../build.mak

LOCAL_PATH	:= $(PJDIR)/pjsip-apps/src/swig/java/android
include $(CLEAR_VARS)

LOCAL_MODULE    := libpjsua2
LOCAL_CFLAGS    := $(APP_CFLAGS) -frtti -fexceptions
LOCAL_LDFLAGS   := $(APP_LDFLAGS)
LOCAL_LDLIBS    := $(APP_LDLIBS)
LOCAL_SRC_FILES := ../output/pjsua2_wrap.cpp

include $(BUILD_SHARED_LIBRARY)
