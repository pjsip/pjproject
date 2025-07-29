include ../../../../build.mak
include ../../../../build/common.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
    OS=android
else
    ifneq ($(findstring ios,$(TARGET_NAME)),)
	OS=ios
    endif
endif
SWIG_FLAGS=-I../../../../pjlib/include \
		   -I../../../../pjlib-util/include \
		   -I../../../../pjmedia/include \
		   -I../../../../pjsip/include \
		   -I../../../../pjnath/include -c++ 
SRC_DIR=../../../../pjsip/include
SRCS=$(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

SWIG_FLAGS += -w312

PROJ_NAME=pjsua2xamarin
OUT_DIR=$(PROJ_NAME)/$(PROJ_NAME)/pjsua2
NAMESPACE=$(PROJ_NAME).pjsua2
MAUI_PROJ_NAME=pjsua2maui
MAUI_NAMESPACE=libpjsua2.maui

ARCH=$(TARGET_ARCH)

ifeq ($(OS),android)
    LIBPJSUA2_DIR=$(PROJ_NAME)/$(PROJ_NAME).Android/lib/$(ARCH)
    LIBPJSUA2=$(LIBPJSUA2_DIR)/libpjsua2.so
    SUP_CLASS_DIR=$(PROJ_NAME)/$(PROJ_NAME).Android/org/pjsip
    MAUI_LIBPJSUA2_DIR=$(MAUI_PROJ_NAME)/$(MAUI_PROJ_NAME)/Platforms/Android/lib/$(ARCH)
    MAUI_LIBPJSUA2=$(MAUI_LIBPJSUA2_DIR)/libpjsua2.so
    MAUI_SUP_CLASS_DIR=$(MAUI_PROJ_NAME)/$(MAUI_PROJ_NAME)/Platforms/Android/org/pjsip
    MAUI_OUT_DIR=$(MAUI_PROJ_NAME)/$(MAUI_PROJ_NAME)/Platforms/Android/pjsua2
else
    ifeq ($(OS),ios)
        LIBPJSUA2_DIR=$(PROJ_NAME)/$(PROJ_NAME).iOS/lib/$(ARCH)
        MAUI_LIBPJSUA2_DIR=$(MAUI_PROJ_NAME)/$(MAUI_PROJ_NAME)/Platforms/iOS/lib/$(ARCH)
        SWIG_FLAGS += -dllimport "__Internal"
        MAUI_OUT_DIR=$(MAUI_PROJ_NAME)/$(MAUI_PROJ_NAME)/Platforms/iOS/pjsua2
    else
        LIBPJSUA2_DIR=$(PROJ_NAME)/lib

    endif
    LIBPJSUA2=$(LIBPJSUA2_DIR)/libpjsua2.a
    MAUI_LIBPJSUA2=$(MAUI_LIBPJSUA2_DIR)/libpjsua2.a
endif

# Build settings
MY_CFLAGS	 := $(PJ_CXXFLAGS) $(CFLAGS)
MY_LDFLAGS	 := $(PJ_LDXXFLAGS) $(PJ_LDXXLIBS) $(LDFLAGS)

.PHONY: all install uninstall

all: $(LIBPJSUA2) $(MAUI_LIBPJSUA2) sample

$(LIBPJSUA2): $(OUT_DIR)/pjsua2_wrap.o
	mkdir -p $(LIBPJSUA2_DIR)
ifeq ($(OS),android)
	mkdir -p $(SUP_CLASS_DIR)
	$(PJ_CXX) -shared -o $(LIBPJSUA2) $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS) $(MY_LDFLAGS)
	# copy libc++_shared.so manually
	cp -f ${STD_CPP_LIB} $(LIBPJSUA2_DIR)
else
	$(AR) $(LIBPJSUA2) $(AR_FLAGS) $(OUT_DIR)/pjsua2_wrap.o $(PJ_LIBXX_FILES)
endif
ifneq (,$(findstring PJMEDIA_VIDEO_DEV_HAS_ANDROID=1,$(ANDROID_CFLAGS)))
	@echo "Copying Android camera helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java $(SUP_CLASS_DIR)/
endif
ifneq (,$(findstring PJMEDIA_AUDIO_DEV_HAS_OBOE=1,$(OBOE_CFLAGS)))
	@echo "Copying Android Oboe audio device helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java $(SUP_CLASS_DIR)/
endif

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp
	$(PJ_CXX) -c $(OUT_DIR)/pjsua2_wrap.cpp -o $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	mkdir -p $(OUT_DIR)
	swig $(SWIG_FLAGS) -namespace $(NAMESPACE) -csharp -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

$(MAUI_LIBPJSUA2): $(MAUI_OUT_DIR)/pjsua2_wrap.o
	mkdir -p $(MAUI_LIBPJSUA2_DIR)
ifeq ($(OS),android)
	mkdir -p $(MAUI_SUP_CLASS_DIR)
	$(PJ_CXX) -shared -o $(MAUI_LIBPJSUA2) $(MAUI_OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS) $(MY_LDFLAGS)
	# copy libc++_shared.so manually
	cp -f ${STD_CPP_LIB} $(MAUI_LIBPJSUA2_DIR)
else
	$(AR) $(MAUI_LIBPJSUA2) $(AR_FLAGS) $(MAUI_OUT_DIR)/pjsua2_wrap.o $(PJ_LIBXX_FILES)
endif
ifneq (,$(findstring PJMEDIA_VIDEO_DEV_HAS_ANDROID=1,$(ANDROID_CFLAGS)))
	@echo "Copying Android camera helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java $(MAUI_SUP_CLASS_DIR)/
endif
ifneq (,$(findstring PJMEDIA_AUDIO_DEV_HAS_OBOE=1,$(OBOE_CFLAGS)))
	@echo "Copying Android Oboe audio device helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java $(MAUI_SUP_CLASS_DIR)/
endif

$(MAUI_OUT_DIR)/pjsua2_wrap.o: $(MAUI_OUT_DIR)/pjsua2_wrap.cpp
	$(PJ_CXX) -c $(MAUI_OUT_DIR)/pjsua2_wrap.cpp -o $(MAUI_OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS)

$(MAUI_OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	mkdir -p $(MAUI_OUT_DIR)
	swig $(SWIG_FLAGS) -namespace $(MAUI_NAMESPACE) -csharp -o $(MAUI_OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

sample: sample.cs
	@echo "Copying sample code..."
	cp sample.cs $(PROJ_NAME)/$(PROJ_NAME)

clean distclean realclean:
	rm -rf ./$(OUT_DIR)/*
	rm -rf ./$(LIBPJSUA2_DIR)/*
	rm -rf ./$(SUP_CLASS_DIR)/*.java

install:

uninstall:
