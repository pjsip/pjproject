include ../../../../build.mak
include ../../../../build/common.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
 OS=android
 ifeq ("$(JAVA_HOME)","")
   # Set dummy JAVA_HOME as JNI is bundled in Android NDK, only need javac here
   JAVA_HOME := $(dir $(shell which javac))
 endif
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

  verify_jdk  = $(shell test -d $(1)/include && test -d $(1)/lib && \
                	echo inclib1) \
                $(shell test -d $(1)/Headers && test -d $(1)/Libraries && \
			echo inclib2) \
                $(shell test -x $(1)/bin/javac && echo bin1) \
                $(shell test -x $(1)/Commands/javac && echo bin2) \
                $(shell test -x $(1)/javac && echo bin3)

  get_java_inc = $(if $(findstring inclib1,$(1)),$(2)/include,\
		 $(if $(findstring inclib2,$(1)),$(2)/Headers,))
  get_java_lib = $(if $(findstring inclib1,$(1)),$(2)/lib,\
		 $(if $(findstring inclib2,$(1)),$(2)/Libraries,))
  get_java_bin = $(if $(findstring bin1,$(1)),$(2)/bin,\
		 $(if $(findstring bin2,$(1)),$(2)/Commands,\
		 $(if $(findstring bin3,$(1)),$(2),)))


  # Check with 'which javac'
  WHICH_JAVAC := $(shell which javac)
  ifneq ($(WHICH_JAVAC),)
    WHICH_JAVAC := $(dir $(realpath $(WHICH_JAVAC)))
    WHICH_JAVAC := $(patsubst %/bin/,%,$(WHICH_JAVAC))
    TMP_RES := $(call verify_jdk,$(WHICH_JAVAC))
    ifneq ($(findstring inclib,$(TMP_RES)),)
      JAVA_HOME := $(WHICH_JAVAC)
      JAVA_INC  := $(call get_java_inc,$(TMP_RES),$(JAVA_HOME))
      JAVA_LIB  := $(call get_java_lib,$(TMP_RES),$(JAVA_HOME))
    endif
  endif

  # MacOS specifics
  ifeq ($(OS)$(JAVA_HOME),darwin)

    # Check with '/usr/libexec/java_home'
    # (see https://developer.apple.com/library/mac/qa/qa1170/_index.html)
    TMP_PATH := $(shell /usr/libexec/java_home)
    ifneq ("$(TMP_PATH)","")
      TMP_RES := $(call verify_jdk,$(TMP_PATH))
      ifneq ($(findstring inclib,$(TMP_RES)),)
        JAVA_HOME := $(TMP_PATH)
        JAVA_INC  := $(call get_java_inc,$(TMP_RES),$(JAVA_HOME))
        JAVA_LIB  := $(call get_java_lib,$(TMP_RES),$(JAVA_HOME))
      endif
    endif

    ifeq ($(JAVA_HOME),)
      # Check '/System/Library/Frameworks/JavaVM.framework'
      FIXED_PATH := /System/Library/Frameworks/JavaVM.framework
      TMP_RES := $(call verify_jdk,$(FIXED_PATH))
      ifneq ($(findstring inclib,$(TMP_RES)),)
        JAVA_HOME := $(FIXED_PATH)
        JAVA_INC  := $(call get_java_inc,$(TMP_RES),$(JAVA_HOME))
        JAVA_LIB  := $(call get_java_lib,$(TMP_RES),$(JAVA_HOME))
      endif
    endif

  endif

  ifneq ($(findstring bin,$(TMP_RES)),)
    JAVA_BIN := $(call get_java_bin,$(TMP_RES),$(JAVA_HOME))
  endif

#  $(info TMP_RES = $(TMP_RES))
#  $(info JAVA_HOME = $(JAVA_HOME))
#  $(info JAVA_INC  = $(JAVA_INC))
#  $(info JAVA_LIB  = $(JAVA_LIB))
#  $(info JAVA_BIN  = $(JAVA_BIN))
#  $(error done)

  ifeq ("$(JAVA_HOME)","")
    $(error Cannot determine JDK include/library path. \
            Please define JAVA_HOME envvar)
  endif

  ifeq ("$(JAVA_BIN)","")
    JAVA_BIN := $(JAVA_HOME)/bin
    $(warning Cannot determine JDK binary path. Set to $(JAVA_BIN))
  endif

else

  JAVA_INC := $(JAVA_HOME)/include
  JAVA_LIB := $(JAVA_HOME)/lib
  ifeq (exists,$(shell test -d "$(JAVA_HOME)/bin" && echo exists))
    JAVA_BIN := $(JAVA_HOME)/bin
  else
    JAVA_BIN := $(JAVA_HOME)
  endif

endif

# Env settings, e.g: path to SWIG, JDK, java(.exe), javac(.exe)
MY_SWIG		 := swig
MY_JAVA	 	 := "$(JAVA_BIN)/java"
MY_JAVAC	 := "$(JAVA_BIN)/javac"
MY_JNI_CFLAGS	 := -I"$(JAVA_INC)" -I"$(JAVA_INC)/win32" \
		    -I"$(JAVA_INC)/linux" -I"$(JAVA_INC)/darwin" -I.
MY_JNI_LDFLAGS	 := -L"$(JAVA_LIB)"

# OS specific
ifeq ($(OS),Windows_NT)
  MY_JNI_LDFLAGS += -Wl,--kill-at
else
  MY_JNI_CFLAGS	 += -fPIC
  ifeq ($(OS),android)
    MY_JNI_CFLAGS+= -D__ANDROID__
    MY_JNI_LDFLAGS+= -Wl,-soname,libpjsua2.so
    SWIG_FLAGS+= -D__ANDROID__
  endif
endif

# Build settings
MY_CFLAGS	 := $(PJ_CXXFLAGS) $(MY_JNI_CFLAGS) $(CFLAGS)
MY_LDFLAGS	 := $(PJ_LDXXFLAGS) $(PJ_LDXXLIBS) $(MY_JNI_LDFLAGS) $(LDFLAGS)
MY_PACKAGE_NAME	 := org.pjsip.pjsua2
ifeq ($(OS),android)
  MY_PACKAGE_PATH  := android/src/$(subst .,/,$(MY_PACKAGE_NAME))
else
  MY_PACKAGE_PATH  := $(OUT_DIR)/$(subst .,/,$(MY_PACKAGE_NAME))
endif

MY_APP_JAVA := android/src/$(subst .,/,$(MY_PACKAGE_NAME))/app/MyApp.java

.PHONY: all java install uninstall

all: $(LIBPJSUA2_SO) java

$(LIBPJSUA2_SO): $(OUT_DIR)/pjsua2_wrap.o
	$(PJ_CXX) -shared -o $(LIBPJSUA2_SO) $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp Makefile
	$(PJ_CXX) -c $(OUT_DIR)/pjsua2_wrap.cpp -o $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i $(SRCS)
	mkdir -p $(MY_PACKAGE_PATH)
	swig $(SWIG_FLAGS) -java  -package $(MY_PACKAGE_NAME) \
		-outdir $(MY_PACKAGE_PATH) \
		-o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(LIBPJSUA2_SO) $(OUT_DIR)/* \
		$(MY_PACKAGE_PATH)/*.java $(MY_PACKAGE_PATH)/*.class

java: $(MY_PACKAGE_PATH)/Error.class $(MY_PACKAGE_PATH)/test.class $(MY_PACKAGE_PATH)/sample.class
ifneq (,$(findstring PJMEDIA_VIDEO_DEV_HAS_ANDROID=1,$(ANDROID_CFLAGS)))
	@echo "Copying Android camera helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java $(MY_PACKAGE_PATH)/..
endif

$(MY_PACKAGE_PATH)/Error.class: $(MY_PACKAGE_PATH)/Error.java
	$(MY_JAVAC) -d $(OUT_DIR) $(MY_PACKAGE_PATH)/*.java $(MY_APP_JAVA)

$(MY_PACKAGE_PATH)/test.class: test.java
	$(MY_JAVAC) -d $(OUT_DIR) -classpath "$(OUT_DIR)" test.java
	
$(MY_PACKAGE_PATH)/sample.class: sample.java
	$(MY_JAVAC) -d $(OUT_DIR) -classpath "$(OUT_DIR)" sample.java

test:
	@# Need to specify classpath and library path, alternatively,
	@# they can be set via CLASSPATH and java.library.path env settings
	$(MY_JAVA) -cp "$(OUT_DIR)" -Djava.library.path="$(OUT_DIR)" test
	
sample:
	@# Need to specify classpath and library path, alternatively,
	@# they can be set via CLASSPATH and java.library.path env settings
	$(MY_JAVA) -cp "$(OUT_DIR)" -Djava.library.path="$(OUT_DIR)" \
		org.pjsip.pjsua2.app.sample	

install:
uninstall:

