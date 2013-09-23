include ../../../../build.mak

OUT_DIR=output
LIBPJSUA2_SO=$(OUT_DIR)/libpjsua2.so

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

# OS specific
ifeq ($(OS),Windows_NT)
MY_JNI_LDFLAGS	 = -L$(MY_JDK)/lib -Wl,--kill-at
MY_JNI_LIB       = $(MY_PACKAGE_BIN)/pjsua.dll
else
MY_JNI_LDFLAGS	 = -L$(MY_JDK)/lib -Wl,-soname,pjsua.so
MY_JNI_LIB       = $(MY_PACKAGE_BIN)/libpjsua.so
MY_JNI_CFLAGS	 := -fPIC
endif

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

all: $(LIBPJSUA2_SO)

$(LIBPJSUA2_SO): $(OUT_DIR)/pjsua2_wrap.o
	gcc -shared -Wl,-soname,libpjsua2.so $(OUT_DIR)/pjsua2_wrap.o -o $(LIBPJSUA2_SO) -lstdc++ $(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp Makefile
	gcc -c $(OUT_DIR)/pjsua2_wrap.cpp -fPIC -o $(OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i $(SRCS)
	swig $(SWIG_FLAGS) -java  -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(OUT_DIR)/pjsua2_wrap.cpp $(OUT_DIR)/pjsua2_wrap.o $(LIBPJSUA2_SO) $(OUT_DIR)/*.java $(OUT_DIR)/*.class
	ln -s ../test.java output/test.java
