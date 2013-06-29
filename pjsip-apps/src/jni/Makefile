# $Id$

# Get PJ build settings
include ../../../build.mak
include $(PJDIR)/build/common.mak

# Env settings, e.g: path to SWIG, JDK, java(.exe), javac(.exe)
MY_SWIG		 = swig
MY_JDK		 = /c/Program\ Files\ \(x86\)/Java/jdk1.7.0_21
MY_JAVA		 = $(MY_JDK)/bin/java.exe
MY_JAVAC	 = $(MY_JDK)/bin/javac.exe
MY_JNI_CFLAGS	 = -I$(MY_JDK)/include -I$(MY_JDK)/include/win32 -I.
MY_JNI_LDFLAGS	 = -L$(MY_JDK)/lib -Wl,--kill-at

# Build settings
MY_CFLAGS	 = $(PJ_CFLAGS) $(MY_JNI_CFLAGS)
MY_LDFLAGS	 = $(PJ_LDFLAGS) $(PJ_LDLIBS) $(MY_JNI_LDFLAGS) -static-libstdc++

# Output/intermediate path settings
MY_PACKAGE	 = org.pjsip.pjsua
MY_OUT_DIR	 = output
MY_SWIG_IF	 = $(MY_OUT_DIR)/pjsua.i
MY_SWIG_FLAG	 = -c++ -I$(MY_OUT_DIR) # -Wall
MY_SWIG_WRAPPER	 = $(MY_OUT_DIR)/pjsua_wrap
MY_PACKAGE_SRC	 = $(MY_OUT_DIR)/src/$(subst .,/,$(MY_PACKAGE))
MY_PACKAGE_BIN	 = $(MY_OUT_DIR)/bin
MY_JNI_LIB       = $(MY_PACKAGE_BIN)/pjsua.dll

all: $(MY_JNI_LIB) java

clean:
	rm -rf $(MY_OUT_DIR)

$(MY_SWIG_IF).tmp: swig_gen.py
	@mkdir -p $(MY_OUT_DIR)
	python swig_gen.py > $(MY_SWIG_IF).tmp

$(MY_SWIG_IF): header.i $(MY_SWIG_IF).tmp
	cat header.i > $(MY_SWIG_IF)
	cat $(MY_SWIG_IF).tmp >> $(MY_SWIG_IF)

$(MY_SWIG_WRAPPER).c: $(MY_SWIG_IF) callbacks.i my_typemaps.i
	@# Cleanup java outdir first, to remove any old/deprecated java files
	rm -rf $(MY_PACKAGE_SRC)
	@mkdir -p $(MY_PACKAGE_SRC)
	$(MY_SWIG) $(MY_SWIG_FLAG) -o $(MY_SWIG_WRAPPER).c -package $(MY_PACKAGE) \
		-outdir $(MY_PACKAGE_SRC) -java $(MY_SWIG_IF) > $(MY_SWIG_WRAPPER)-tm.log

$(MY_JNI_LIB): $(MY_SWIG_WRAPPER).c
	@mkdir -p $(MY_PACKAGE_BIN)
	$(PJ_CXX) -shared -o $(MY_JNI_LIB) $(MY_SWIG_WRAPPER).c $(MY_OUT_DIR)/callbacks.c \
		$(MY_CFLAGS) $(MY_LDFLAGS)

java: hello.java
	@mkdir -p $(MY_PACKAGE_BIN)
	$(MY_JAVAC) -d $(MY_PACKAGE_BIN) $(MY_PACKAGE_SRC)/*.java
	$(MY_JAVAC) -d $(MY_PACKAGE_BIN) -classpath "$(MY_PACKAGE_BIN)" hello.java

test: $(MY_PACKAGE_BIN)/hello.class
	@# Need to specify classpath and library path, alternatively, they can be set via
	@# CLASSPATH and java.library.path env settings
	$(MY_JAVA) -cp $(MY_PACKAGE_BIN) -Djava.library.path="$(MY_PACKAGE_BIN)" hello
