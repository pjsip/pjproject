include ../../../../build.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
 OS=android
else
 ifneq ($(findstring darwin,$(TARGET_NAME)),)
  OS=darwin
 endif
endif

OUT_DIR=output
ifeq ($(OS),Windows_NT)
  LIBPJSUA2_SO=$(OUT_DIR)/pjsua2.dll
else
  ifeq ($(OS),darwin)
    LIBPJSUA2_SO=$(OUT_DIR)/libpjsua2.jnilib
  else
    ifeq ($(OS),android)
      LIBPJSUA2_SO=android/libs/armeabi/libpjsua2.so
    else
      LIBPJSUA2_SO=$(OUT_DIR)/libpjsua2.so
    endif
  endif
endif

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
else
  ifeq (exists, $(shell test -d $(JAVA_HOME)/bin && echo exists ))
    JAVA_BIN := $(JAVA_HOME)/bin
  else
    JAVA_BIN := $(JAVA_HOME)
  endif
endif

# OS specific
ifeq ($(OS),Windows_NT)
  MY_JNI_LDFLAGS	= -L$(MY_JDK)/lib -Wl,--kill-at
else
  MY_JNI_CFLAGS	 	= -fPIC
  MY_JNI_LDFLAGS	= -L$(MY_JDK)/lib
  ifeq ($(OS),darwin)
    MY_JNI_LDFLAGS 	:= $(MY_JNI_LDFLAGS) -Wl,-soname,pjsua2.so
  endif
  ifeq ($(OS),android)
    MY_JNI_CFLAGS	:= $(MY_JNI_CFLAGS) -D__ANDROID__
  endif
endif

# Env settings, e.g: path to SWIG, JDK, java(.exe), javac(.exe)
MY_SWIG		 = swig
MY_JDK		 = $(JAVA_HOME)
ifneq ($(findstring bin,$(JAVA_BIN)),)
  MY_JAVA	 = $(MY_JDK)/bin/java
  MY_JAVAC	 = $(MY_JDK)/bin/javac
else
  MY_JAVA	 = $(MY_JDK)/java
  MY_JAVAC	 = $(MY_JDK)/javac
endif
MY_JNI_CFLAGS	 := $(MY_JNI_CFLAGS) -I$(MY_JDK)/include -I$(MY_JDK)/include/win32 \
		    -I$(MY_JDK)/include/linux -I.

# Build settings
MY_CFLAGS	 = $(PJ_CFLAGS) $(MY_JNI_CFLAGS)
MY_LDFLAGS	 = $(PJ_LDFLAGS) -lpjsua2-$(TARGET_NAME) $(PJ_LDLIBS) $(MY_JNI_LDFLAGS)
MY_PACKAGE_NAME	 = org.pjsip.pjsua2
ifeq ($(OS),android)
  MY_PACKAGE_PATH  = android/src/$(subst .,/,$(MY_PACKAGE_NAME))
else
  MY_PACKAGE_PATH  = $(OUT_DIR)/$(subst .,/,$(MY_PACKAGE_NAME))
endif

.PHONY: all java install uninstall

all: $(LIBPJSUA2_SO) java

$(LIBPJSUA2_SO): $(OUT_DIR)/pjsua2_wrap.o
	$(PJ_CXX) -shared -o $(LIBPJSUA2_SO) $(OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp Makefile
	$(PJ_CXX) -c $(OUT_DIR)/pjsua2_wrap.cpp -o $(OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i $(SRCS)
	mkdir -p $(MY_PACKAGE_PATH)
	swig $(SWIG_FLAGS) -java  -package $(MY_PACKAGE_NAME) -outdir $(MY_PACKAGE_PATH) -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(LIBPJSUA2_SO) $(OUT_DIR)/* $(MY_PACKAGE_PATH)/*.java $(MY_PACKAGE_PATH)/*.class

java: $(MY_PACKAGE_PATH)/Error.class $(MY_PACKAGE_PATH)/test.class

$(MY_PACKAGE_PATH)/Error.class: $(MY_PACKAGE_PATH)/Error.java
	$(MY_JAVAC) -d $(OUT_DIR) $(MY_PACKAGE_PATH)/*.java

$(MY_PACKAGE_PATH)/test.class: test.java
	$(MY_JAVAC) -d $(OUT_DIR) -classpath "$(OUT_DIR)" test.java

test:
	@# Need to specify classpath and library path, alternatively, they can be set via
	@# CLASSPATH and java.library.path env settings
	$(MY_JAVA) -cp "$(OUT_DIR);$(MY_PACKAGE_PATH)" -Djava.library.path="$(OUT_DIR)" test

install:
uninstall:

