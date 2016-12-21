include ../../../../../build.mak
include ../../../../../build/common.mak

MY_MODULE_PATH  := $(PJDIR)/pjsip-apps/build/output/pjsua-$(TARGET_NAME)
MY_MODULES      := $(MY_MODULE_PATH)/pjsua_app.o \
		   $(MY_MODULE_PATH)/pjsua_app_cli.o \
		   $(MY_MODULE_PATH)/pjsua_app_common.o \
		   $(MY_MODULE_PATH)/pjsua_app_config.o \
		   $(MY_MODULE_PATH)/pjsua_app_legacy.o

OUT_DIR		:= ../build/jni
LIBPJSUA_SO	:= ../app/src/main/jniLibs/armeabi/libpjsua.so

# Env settings, e.g: path to SWIG, JDK, java(.exe), javac(.exe)
MY_SWIG		:= swig
MY_JNI_CFLAGS   := -D__ANDROID__ -fPIC
MY_JNI_LDFLAGS  := -Wl,-soname,libpjsua.so
SWIG_FLAGS      := -D__ANDROID__ -c++

# Build settings
MY_CFLAGS	:= $(PJ_CXXFLAGS) $(MY_JNI_CFLAGS) $(CFLAGS)
MY_LDFLAGS	:= $(PJ_LDXXFLAGS) $(PJ_LDXXLIBS) $(MY_JNI_LDFLAGS) $(LDFLAGS)
MY_PACKAGE_NAME	:= org.pjsip.pjsua
MY_PACKAGE_PATH := ../app/src/main/java/$(subst .,/,$(MY_PACKAGE_NAME))

all: $(LIBPJSUA_SO) java

$(LIBPJSUA_SO): $(OUT_DIR)/pjsua_wrap.o
	mkdir -p ../app/src/main/jniLibs/armeabi
	$(PJ_CXX) -shared -o $(LIBPJSUA_SO) \
	$(OUT_DIR)/pjsua_wrap.o $(OUT_DIR)/pjsua_app_callback.o \
	$(MY_MODULES) \
		$(MY_CFLAGS) $(MY_LDFLAGS)

$(OUT_DIR)/pjsua_wrap.o: $(OUT_DIR)/pjsua_wrap.cpp pjsua_app_callback.cpp Makefile
	$(PJ_CXX) -c $(OUT_DIR)/pjsua_wrap.cpp -o $(OUT_DIR)/pjsua_wrap.o \
		$(MY_CFLAGS)
	$(PJ_CXX) -c pjsua_app_callback.cpp -o $(OUT_DIR)/pjsua_app_callback.o \
		$(MY_CFLAGS)

$(OUT_DIR)/pjsua_wrap.cpp: pjsua.i
	mkdir -p $(MY_PACKAGE_PATH)
	mkdir -p $(OUT_DIR)
	swig $(SWIG_FLAGS) -java  -package $(MY_PACKAGE_NAME) \
		-outdir $(MY_PACKAGE_PATH) \
		-o $(OUT_DIR)/pjsua_wrap.cpp pjsua.i

clean distclean realclean:
	rm -rf $(LIBPJSUA_SO) $(OUT_DIR)/* \
		$(MY_PACKAGE_PATH)/*.java

java:
ifneq (,$(findstring PJMEDIA_VIDEO_DEV_HAS_ANDROID=1,$(ANDROID_CFLAGS)))
	@echo "Copying Android camera helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java $(MY_PACKAGE_PATH)/..
endif
