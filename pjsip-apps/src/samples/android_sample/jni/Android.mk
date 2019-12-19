LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := dummy_static
LOCAL_SRC_FILES := dummy.c dummy2.cc
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE	:= dummy_shared
LOCAL_STATIC_LIBRARIES := dummy_static
LOCAL_SRC_FILES := dummy.c dummy2.cc
include $(BUILD_SHARED_LIBRARY)
