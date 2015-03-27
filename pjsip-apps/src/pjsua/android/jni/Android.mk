# $Id$

LOCAL_PATH	:= $(call my-dir)
include $(CLEAR_VARS)

# Get PJ build settings
include ../../../../build.mak
include $(PJDIR)/build/common.mak

# Path to SWIG
MY_SWIG		:= swig

MY_MODULE_PATH  := $(PJDIR)/pjsip-apps/build/output/pjsua-$(TARGET_NAME)
MY_MODULES      := $(MY_MODULE_PATH)/pjsua_app.o \
		   $(MY_MODULE_PATH)/pjsua_app_cli.o \
		   $(MY_MODULE_PATH)/pjsua_app_common.o \
		   $(MY_MODULE_PATH)/pjsua_app_config.o \
		   $(MY_MODULE_PATH)/pjsua_app_legacy.o

# Constants
MY_JNI_WRAP	:= pjsua_wrap.cpp
MY_JNI_DIR	:= jni

# Android build settings
LOCAL_MODULE    := libpjsua
LOCAL_CFLAGS    := -Werror $(APP_CFLAGS) -frtti
LOCAL_LDFLAGS   := $(APP_LDFLAGS)
LOCAL_LDLIBS    := $(MY_MODULES) $(APP_LDLIBS)
LOCAL_SRC_FILES := $(MY_JNI_WRAP) pjsua_app_callback.cpp

# Copy Java Camera helper components from pjmedia/src/pjmedia-videodev/android
src/org/pjsip/PjCamera.java:
ifneq (,$(findstring PJMEDIA_VIDEO_DEV_HAS_ANDROID=1,$(ANDROID_CFLAGS)))
	@echo "Copying Android camera helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java src/org/pjsip
endif

# Invoke SWIG
$(MY_JNI_DIR)/$(MY_JNI_WRAP): src/org/pjsip/PjCamera.java
	@echo "Invoking SWIG..."
	$(MY_SWIG) -c++ -o $(MY_JNI_DIR)/$(MY_JNI_WRAP) -package org.pjsip.pjsua -outdir src/org/pjsip/pjsua -java $(MY_JNI_DIR)/pjsua.i

.PHONY: $(MY_JNI_DIR)/$(MY_JNI_WRAP)

include $(BUILD_SHARED_LIBRARY)


# Add prebuilt libopenh264, if configured
ifneq (,$(OPENH264_LDFLAGS))
include $(CLEAR_VARS)
LOCAL_MODULE := libopenh264
LOCAL_SRC_FILES := $(subst -L,,$(filter -L%,$(OPENH264_LDFLAGS)))/libopenh264.so
include $(PREBUILT_SHARED_LIBRARY)
endif

# Add prebuilt libyuv, if configured
ifneq (,$(LIBYUV_LDFLAGS))
include $(CLEAR_VARS)
LOCAL_MODULE := libyuv
LOCAL_SRC_FILES := $(subst -L,,$(filter -L%,$(LIBYUV_LDFLAGS)))/libyuv.so
include $(PREBUILT_SHARED_LIBRARY)
endif
