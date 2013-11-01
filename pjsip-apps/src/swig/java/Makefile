include ../../../../build.mak

OUT_DIR=output
ifeq ($(OS),Windows_NT)
LIBPJSUA2_SO=$(OUT_DIR)/pjsua2.dll
else
LIBPJSUA2_SO=$(OUT_DIR)/libpjsua2.so
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
endif

# OS specific
ifeq ($(OS),Windows_NT)
MY_JNI_LDFLAGS	 = -L$(MY_JDK)/lib -Wl,--kill-at
MY_JNI_LIB       = $(OUT_DIR)/pjsua2.dll
else
MY_JNI_LDFLAGS	 = -L$(MY_JDK)/lib -Wl,-soname,pjsua2.so
MY_JNI_LIB       = $(OUT_DIR)/libpjsua2.so
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
MY_LDFLAGS	 = $(PJ_LDFLAGS) -lpjsua2-$(TARGET_NAME) $(PJ_LDLIBS) $(MY_JNI_LDFLAGS) -static-libstdc++

.PHONY: all java install uninstall

all: $(LIBPJSUA2_SO) java

$(LIBPJSUA2_SO): $(OUT_DIR)/pjsua2_wrap.o
	g++ -shared -o $(LIBPJSUA2_SO) $(OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp Makefile
	g++ -c $(OUT_DIR)/pjsua2_wrap.cpp -o $(OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i $(SRCS)
	swig $(SWIG_FLAGS) -java  -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(OUT_DIR)/pjsua2_wrap.cpp $(OUT_DIR)/pjsua2_wrap.o $(LIBPJSUA2_SO) $(OUT_DIR)/*.java $(OUT_DIR)/*.class

java: output/Error.class output/test.class

output/Error.class: output/Error.java
	$(MY_JAVAC) -d $(OUT_DIR) $(OUT_DIR)/*.java

output/test.class: test.java
	$(MY_JAVAC) -d $(OUT_DIR) -classpath "$(OUT_DIR)" test.java

test:
	@# Need to specify classpath and library path, alternatively, they can be set via
	@# CLASSPATH and java.library.path env settings
	$(MY_JAVA) -cp $(OUT_DIR) -Djava.library.path="$(OUT_DIR)" test

install:
uninstall:

