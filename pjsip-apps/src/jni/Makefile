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

# Choose C or C++ SWIG wrapper target, just for dev purpose.
# Note that C target doesn't support callback, and C++ target doesn't support nested class/struct.
SWIG_CPP_TARGET = 0

# Build settings
MY_CFLAGS	 = $(PJ_CFLAGS) $(MY_JNI_CFLAGS)
MY_LDFLAGS	 = $(PJ_LDFLAGS) $(PJ_LDLIBS) $(MY_JNI_LDFLAGS)

# Output/intermediate path settings
MY_PACKAGE	 = org.pjsip.pjsua
MY_OUT_DIR	 = output
MY_SWIG_IF	 = $(MY_OUT_DIR)/pjsua.i
MY_SWIG_FLAG	 = -Wall
MY_SWIG_WRAPPER	 = $(MY_OUT_DIR)/pjsua_wrap
MY_CALLBACKS	 = callbacks
MY_PACKAGE_SRC	 = $(MY_OUT_DIR)/src/$(subst .,/,$(MY_PACKAGE))
MY_PACKAGE_BIN	 = $(MY_OUT_DIR)/bin
MY_JNI_LIB       = $(MY_PACKAGE_BIN)/pjsua.dll

# Adjust setting for C/C++
ifeq ($(SWIG_CPP_TARGET),1)
MY_CC		 = $(PJ_CXX)
MY_LDFLAGS	+= -static-libstdc++
MY_SWIG_FLAG    += -c++ -debug-tmsearch -debug-tmused
else
MY_CC		 = $(PJ_CC)
MY_SWIG_FLAG    += -debug-tmsearch -debug-tmused
endif

all: $(MY_JNI_LIB) java

clean:
	rm -rf $(MY_OUT_DIR)

$(MY_SWIG_IF): header.i $(MY_SWIG_IF).tmp
	cat header.i > $(MY_SWIG_IF)
	cat $(MY_SWIG_IF).tmp >> $(MY_SWIG_IF)

$(MY_SWIG_IF).tmp: swig_gen.py
	@mkdir -p $(MY_OUT_DIR)
	python swig_gen.py > $(MY_SWIG_IF).tmp

$(MY_SWIG_WRAPPER).c: $(MY_SWIG_IF) $(MY_CALLBACKS).h $(MY_CALLBACKS).cpp $(MY_CALLBACKS).i
	# Cleanup java outdir first, to remove any old/deprecated java files
	rm -rf $(MY_PACKAGE_SRC)
	@mkdir -p $(MY_PACKAGE_SRC)
	$(MY_SWIG) $(MY_SWIG_FLAG) -o $(MY_SWIG_WRAPPER).c -package $(MY_PACKAGE) \
		-outdir $(MY_PACKAGE_SRC) -java $(MY_SWIG_IF) > $(MY_SWIG_WRAPPER)-tm.txt
	$(MY_SWIG) -c++ $(MY_SWIG_FLAG) -o $(MY_SWIG_WRAPPER)2.c -package $(MY_PACKAGE) \
		-outdir $(MY_PACKAGE_SRC) -java $(MY_CALLBACKS).i > $(MY_SWIG_WRAPPER)2-tm.txt

$(MY_SWIG_WRAPPER).o: $(MY_SWIG_WRAPPER).c
	$(MY_CC) -c $(MY_CFLAGS) $(MY_SWIG_WRAPPER).c -o $(MY_SWIG_WRAPPER).o
	$(PJ_CXX) -c $(MY_CFLAGS) $(MY_SWIG_WRAPPER)2.c -o $(MY_SWIG_WRAPPER)2.o
	
$(MY_OUT_DIR)/$(MY_CALLBACKS).o: $(MY_CALLBACKS).cpp $(MY_CALLBACKS).h
	$(PJ_CXX) -c $(MY_CFLAGS) $(MY_CALLBACKS).cpp -o $(MY_OUT_DIR)/$(MY_CALLBACKS).o

$(MY_JNI_LIB): $(MY_SWIG_WRAPPER).o $(MY_OUT_DIR)/$(MY_CALLBACKS).o
	@mkdir -p $(MY_PACKAGE_BIN)
	$(PJ_CXX) -shared -o $(MY_JNI_LIB) $(MY_SWIG_WRAPPER).o $(MY_OUT_DIR)/$(MY_CALLBACKS).o \
		$(MY_SWIG_WRAPPER)2.o $(MY_LDFLAGS) -static-libstdc++

java: hello.java
	@mkdir -p $(MY_PACKAGE_BIN)
	$(MY_JAVAC) -d $(MY_PACKAGE_BIN) $(MY_PACKAGE_SRC)/*.java
	$(MY_JAVAC) -d $(MY_PACKAGE_BIN) -classpath "$(MY_PACKAGE_BIN)" hello.java

test: $(MY_PACKAGE_BIN)/hello.class
	# Need to specify classpath and library path, alternatively, they can be set via
	# CLASSPATH and java.library.path env settings
	$(MY_JAVA) -cp $(MY_PACKAGE_BIN) -Djava.library.path="$(MY_PACKAGE_BIN)" hello
