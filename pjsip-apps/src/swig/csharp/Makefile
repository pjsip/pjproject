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

ARCH=$(TARGET_ARCH)

ifeq ($(OS),android)
    LIBPJSUA2_DIR=$(PROJ_NAME)/Droid/lib/$(ARCH)
    LIBPJSUA2=$(LIBPJSUA2_DIR)/libpjsua2.so
endif
ifeq ($(OS),ios)
    LIBPJSUA2_DIR=$(PROJ_NAME)/iOS/lib/$(ARCH)
    LIBPJSUA2=$(LIBPJSUA2_DIR)/libpjsua2.a
endif

# Build settings
MY_CFLAGS	 := $(PJ_CXXFLAGS) $(CFLAGS)
MY_LDFLAGS	 := $(PJ_LDXXFLAGS) $(PJ_LDXXLIBS) $(LDFLAGS)

.PHONY: all install uninstall

all: $(LIBPJSUA2) sample

$(LIBPJSUA2): $(OUT_DIR)/pjsua2_wrap.o
	mkdir -p $(LIBPJSUA2_DIR)
ifeq ($(OS),android)
	$(PJ_CXX) -shared -o $(LIBPJSUA2) $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS) $(MY_LDFLAGS)
endif
ifeq ($(OS),ios)
	$(AR) $(LIBPJSUA2) $(AR_FLAGS) $(OUT_DIR)/pjsua2_wrap.o $(PJ_LIBXX_FILES)
endif

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp
	$(PJ_CXX) -c $(OUT_DIR)/pjsua2_wrap.cpp -o $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	mkdir -p $(OUT_DIR)
	swig $(SWIG_FLAGS) -namespace $(NAMESPACE) -csharp -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i
	
sample: sample.cs
	@echo "Copying sample code..."
	cp sample.cs $(PROJ_NAME)/$(PROJ_NAME)

clean distclean realclean:
	rm -rf $(OUT_DIR)/*
	rm -rf $(LIBPJSUA2_DIR)/*

install:

uninstall:
