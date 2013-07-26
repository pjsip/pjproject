# $Id: Makefile 4563 2013-07-15 05:34:14Z bennylp $

# Get PJ build settings
include ../../../build.mak
include $(PJDIR)/build/common.mak

# Get JDK location
ifeq ("$(JAVA_HOME)","")
  # Get javac location to determine JDK location
  JAVAC_PATH = $(shell which javac)
  ifeq ("$(JAVAC_PATH)","")
    $(error Cannot determine JDK location using 'which' command. Please define JAVA_HOME envvar)
  endif

  JAVAC_PATH := $(realpath $(JAVAC_PATH))
  JAVA_BIN := $(dir $(JAVAC_PATH))
  JAVA_HOME := $(patsubst %/bin/,%,$(JAVA_BIN))
endif

MY_JNI_LDFLAGS	 = -L$(MY_JDK)/lib -Wl,-soname,pjsua.so
MY_JNI_LIB       = $(MY_PACKAGE_BIN)/libpjsua.so
MY_JNI_CFLAGS	 := -fPIC

# Env settings, e.g: path to SWIG, JDK, java(.exe), javac(.exe)
MY_SWIG		 = swig
MY_JDK		 = $(JAVA_HOME)
MY_JAVA		 = $(MY_JDK)/bin/java
MY_JAVAC	 = $(MY_JDK)/bin/javac
MY_JNI_CFLAGS	 := $(MY_JNI_CFLAGS) -I$(MY_JDK)/include -I$(MY_JDK)/include/win32 \
		   -I$(MY_JDK)/include/linux -I.

# Build settings
MY_CFLAGS	 = $(PJ_CFLAGS) $(MY_JNI_CFLAGS)
MY_LDFLAGS	 = $(PJ_LDFLAGS) $(PJ_LDLIBS) $(MY_JNI_LDFLAGS) -static-libstdc++

# Output/intermediate path settings
MY_PACKAGE	 = org.pjsip.pjsua
MY_OUT_DIR	 = jni/output
MY_SWIG_IF	 = $(MY_OUT_DIR)/pjsua.i
MY_SWIG_FLAG	 = -c++ -I$(MY_OUT_DIR)
MY_SWIG_WRAPPER	 = $(MY_OUT_DIR)/pjsua_wrap
MY_PACKAGE_SRC	 = src/$(subst .,/,$(MY_PACKAGE))
MY_PACKAGE_BIN	 = $(MY_OUT_DIR)/bin

TEST_PACKAGE	 = org.pjsip.hello
TEST_SRC	 = src/$(subst .,/,$(TEST_PACKAGE))

# Android build settings
LOCAL_PATH	:= $(PJDIR)/pjsip-apps/src/jni
include $(CLEAR_VARS)
LOCAL_MODULE    := libpjsua
LOCAL_MODULE_FILENAME := libpjsua
LOCAL_CFLAGS    := -Werror $(APP_CFLAGS) -frtti
LOCAL_LDFLAGS   := $(APP_LDFLAGS)
LOCAL_LDLIBS    := $(APP_LDLIBS)
LOCAL_SRC_FILES := $(MY_SWIG_WRAPPER).cpp $(MY_OUT_DIR)/callbacks.cpp

jni: $(MY_JNI_LIB) java

clean:
	rm -rf $(MY_SWIG_WRAPPER).*
	rm -rf $(MY_PACKAGE_SRC)

$(MY_SWIG_WRAPPER).cpp: $(MY_SWIG_IF) jni/callbacks.i jni/my_typemaps.i
	@# Cleanup java outdir first, to remove any old/deprecated java files
	rm -rf $(MY_PACKAGE_SRC)
	@mkdir -p $(MY_PACKAGE_SRC)
	$(MY_SWIG) $(MY_SWIG_FLAG) -o $(MY_SWIG_WRAPPER).cpp -package $(MY_PACKAGE) \
		-outdir $(MY_PACKAGE_SRC) -java $(MY_SWIG_IF)

$(MY_JNI_LIB): $(MY_SWIG_WRAPPER).cpp
	@mkdir -p $(MY_PACKAGE_BIN)
	$(PJ_CXX) -shared -o $(MY_JNI_LIB) $(MY_SWIG_WRAPPER).cpp $(MY_OUT_DIR)/callbacks.cpp \
		$(MY_CFLAGS) $(MY_LDFLAGS)

$(LOCAL_PATH)/$(MY_SWIG_WRAPPER).cpp: $(MY_SWIG_WRAPPER).cpp

$(LOCAL_PATH)/$(MY_OUT_DIR)/callbacks.cpp: $(MY_OUT_DIR)/callbacks.cpp

include $(BUILD_SHARED_LIBRARY)
