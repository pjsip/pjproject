# $id$

LOCAL_PATH	:= $(call my-dir)
include $(CLEAR_VARS)

# Get PJ build settings
include ../../../../build.mak
include $(PJDIR)/build/common.mak

# Path to SWIG
MY_SWIG		:= swig

#========================================================================
# Choose console application to load
#========================================================================

# pjlib test
#MY_MODULE :=  $(PJDIR)/pjlib/build/output/pjlib-test-$(TARGET_NAME)/*.o

# pjlib-util test
#MY_MODULE :=  $(PJDIR)/pjlib-util/build/output/pjlib-util-test-$(TARGET_NAME)/*.o

# pjsip test
#MY_MODULE :=  $(PJDIR)/pjsip/build/output/pjsip-test-$(TARGET_NAME)/*.o

# pjnath test
#MY_MODULE :=  $(PJDIR)/pjnath/build/output/pjnath-test-$(TARGET_NAME)/*.o

# pjmedia test
# Note: jbuf test requires Jbtest.dat, this jbuf test must be disabled (for now).
#MY_MODULE :=  $(PJDIR)/pjmedia/build/output/pjmedia-test-$(TARGET_NAME)/*.o

# pjsystest app (not supported yet)
# Todo: this test requires some input and output files (log & WAV).
#MY_MODULE := $(PJDIR)/pjsip-apps/build/output/pjsystest-$(TARGET_NAME)/*.o

# pjsua app
# Note: must set USE_GUI to zero in config_site.h
MY_MODULE := $(PJDIR)/pjsip-apps/build/output/pjsua-$(TARGET_NAME)/*.o

#========================================================================

# Constants
MY_JNI_WRAP	:= pjsua_wrap.cpp
MY_JNI_DIR	:= jni

# Android build settings
LOCAL_MODULE    := libpjsua
LOCAL_CFLAGS    := -Werror $(APP_CFLAGS) -frtti
LOCAL_LDFLAGS   := $(APP_LDFLAGS)
LOCAL_LDLIBS    := $(MY_MODULE) $(APP_LDLIBS)
LOCAL_SRC_FILES := $(MY_JNI_WRAP) pjsua_app_callback.cpp

# Invoke SWIG
$(MY_JNI_DIR)/$(MY_JNI_WRAP):
	@echo "Invoking SWIG..."
	$(MY_SWIG) -c++ -o $(MY_JNI_DIR)/$(MY_JNI_WRAP) -package org.pjsip.pjsua -outdir src/org/pjsip/pjsua -java $(MY_JNI_DIR)/pjsua.i

.PHONY: $(MY_JNI_DIR)/$(MY_JNI_WRAP)

include $(BUILD_SHARED_LIBRARY)
