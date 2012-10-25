# $Id$

LOCAL_PATH	:= $(call my-dir)
include $(CLEAR_VARS)

# Get PJ build settings
include ../../../build.mak
include $(PJDIR)/build/common.mak

# Path to SWIG
MY_SWIG		:= swig

# Constants
MY_JNI_WRAP	:= pjsua_app_wrap.c
MY_JNI_DIR	:= jni
MY_MODULE 	:= $(PJDIR)/pjsip-apps/build/output/pjsua-$(TARGET_NAME)/*.o

# Android build settings
LOCAL_MODULE    := libpjsua_app
LOCAL_CFLAGS    := -Werror $(APP_CFLAGS)
LOCAL_LDFLAGS   := $(APP_LDFLAGS)
LOCAL_LDLIBS    := $(MY_MODULE) $(APP_LDLIBS)
LOCAL_SRC_FILES := $(MY_JNI_WRAP)

# Invoke SWIG
$(MY_JNI_DIR)/$(MY_JNI_WRAP):
	@echo "Invoking SWIG..."
	$(MY_SWIG) -o $(MY_JNI_DIR)/$(MY_JNI_WRAP) -package org.pjsip.pjsua -outdir src/org/pjsip/pjsua -java $(MY_JNI_DIR)/pjsua_app.i

.PHONY: $(MY_JNI_DIR)/$(MY_JNI_WRAP)

include $(BUILD_SHARED_LIBRARY)

