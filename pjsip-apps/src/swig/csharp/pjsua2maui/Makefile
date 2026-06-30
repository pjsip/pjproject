include ../../../../../build.mak
include ../../../../../build/common.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
    MAUI_OS = android
    LIB_NAME = libpjsua2.so
else ifneq ($(findstring ios,$(TARGET_NAME)),)
    MAUI_OS = ios
    LIB_NAME = libpjsua2.a
else ifneq ($(findstring macabi,$(APP_CFLAGS)),)
    MAUI_OS = maccatalyst
    LIB_NAME = libpjsua2.a
else
    MAUI_OS = desktop
    LIB_NAME = libpjsua2.a
endif

SWIG_FLAGS = -I../../../../../pjlib/include \
             -I../../../../../pjlib-util/include \
             -I../../../../../pjmedia/include \
             -I../../../../../pjsip/include \
             -I../../../../../pjnath/include -c++ -w312 -csharp

SRC_DIR = ../../../../../pjsip/include
SRCS = $(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

MAUI_PROJ_NAME = pjsua2maui
MAUI_NAMESPACE = libpjsua2.maui

ifeq ($(MAUI_OS),android)
    ARCH_DIR = $(TARGET_ARCH)
    ifeq ($(ARCH_DIR),)
        ARCH_DIR = x86_64
    endif
    PLATFORM_DIR = Android
    MAUI_SUP_CLASS_DIR = $(MAUI_PROJ_NAME)/Platforms/Android/org/pjsip
else
    ifeq ($(MAUI_OS),maccatalyst)
        PLATFORM_DIR = MacCatalyst
        SWIG_FLAGS += -dllimport "__Internal"
    else
        PLATFORM_DIR = iOS
        SWIG_FLAGS += -dllimport "__Internal"
    endif
    
    # arch extraction from flags
    ifneq ($(findstring arm64,$(APP_CFLAGS)),)
        ARCH_DIR = arm64
    else
        ARCH_DIR = x86_64
    endif
endif

OUTPUT_PLATFORM_DIR = $(MAUI_PROJ_NAME)/Platforms/$(PLATFORM_DIR)
MAUI_OUT_DIR        = $(OUTPUT_PLATFORM_DIR)/pjsua2
MAUI_LIBPJSUA2_DIR  = $(OUTPUT_PLATFORM_DIR)/lib/$(ARCH_DIR)
MAUI_LIBPJSUA2      = $(MAUI_LIBPJSUA2_DIR)/$(LIB_NAME)

MY_CFLAGS   := $(PJ_CXXFLAGS) $(CFLAGS)
MY_LDFLAGS  := $(PJ_LDXXFLAGS) $(PJ_LDXXLIBS) $(LDFLAGS)

.PHONY: all clean dep depend distclean realclean install uninstall

all: $(MAUI_LIBPJSUA2)

# Generate/Link rule for final library 
$(MAUI_LIBPJSUA2): $(MAUI_OUT_DIR)/pjsua2_wrap.o
	mkdir -p $(MAUI_LIBPJSUA2_DIR)
ifeq ($(MAUI_OS),android)
	mkdir -p $(MAUI_SUP_CLASS_DIR)
	$(PJ_CXX) -shared -o $(MAUI_LIBPJSUA2) $(MAUI_OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS) $(MY_LDFLAGS)
	# helper C++ library copy 
	if [ -f "${STD_CPP_LIB}" ]; then cp -f ${STD_CPP_LIB} $(MAUI_LIBPJSUA2_DIR); fi
	# Android specific audio and video helpers
	if echo "$(MY_CFLAGS)" | grep -q "PJMEDIA_VIDEO_DEV_HAS_ANDROID=1"; then \
		cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java $(MAUI_SUP_CLASS_DIR)/; \
	fi
	if echo "$(MY_CFLAGS)" | grep -q "PJMEDIA_AUDIO_DEV_HAS_OBOE=1"; then \
		cp $(PJDIR)/pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java $(MAUI_SUP_CLASS_DIR)/; \
	fi
else
	# CORREÇÃO DA APPLE (iOS/MacCatalyst): Uso do libtool nativo para unificar de forma limpa o wrapper e as libs do PJSIP
	libtool -static -o $(MAUI_LIBPJSUA2) $(MAUI_OUT_DIR)/pjsua2_wrap.o $(PJ_LIBXX_FILES)
endif

# building C++ wrapper using swig
$(MAUI_OUT_DIR)/pjsua2_wrap.o: $(MAUI_OUT_DIR)/pjsua2_wrap.cpp
	$(PJ_CXX) -c $(MAUI_OUT_DIR)/pjsua2_wrap.cpp -o $(MAUI_OUT_DIR)/pjsua2_wrap.o $(MY_CFLAGS)

# Generate managed C# classes using swig
$(MAUI_OUT_DIR)/pjsua2_wrap.cpp: ../../pjsua2.i ../../symbols.i Makefile $(SRCS)
	mkdir -p $(MAUI_OUT_DIR)
	swig $(SWIG_FLAGS) -namespace $(MAUI_NAMESPACE) -o $(MAUI_OUT_DIR)/pjsua2_wrap.cpp ../../pjsua2.i

clean distclean realclean:
	rm -rf $(MAUI_OUT_DIR)
	rm -rf $(MAUI_LIBPJSUA2_DIR)
ifeq ($(MAUI_OS),android)
	rm -rf $(MAUI_SUP_CLASS_DIR)/*.java
endif

dep depend install uninstall: