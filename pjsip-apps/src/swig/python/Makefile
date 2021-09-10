PYTHON_SO=_pjsua2.so

USE_PYTHON3?=1

ifeq ($(USE_PYTHON3),1)
  PYTHON_EXE=python3
else
  PYTHON_EXE=python2
endif

#PYTHON_SETUP_FLAGS = --inplace 
ifeq ($(OS),Windows_NT)
  PYTHON_PKG_DIR := $(shell $(PYTHON_EXE) -m site --user-site | sed 's/\\/\//g')
  PYTHON_SETUP_FLAGS += --compiler=mingw32
  GCC_EXE=gcc.exe
else
  PYTHON_PKG_DIR := $(shell $(PYTHON_EXE) -m site --user-site)
  GCC_EXE=
endif

SWIG_FLAGS=-I../../../../pjlib/include \
		   -I../../../../pjlib-util/include \
		   -I../../../../pjmedia/include \
		   -I../../../../pjsip/include \
		   -I../../../../pjnath/include -c++ 
SRC_DIR=../../../../pjsip/include
SRCS=$(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

#USE_THREADS = -threads -DSWIG_NO_EXPORT_ITERATOR_METHODS
SWIG_FLAGS += -w312 $(USE_THREADS)

.PHONY: all install uninstall

all: $(PYTHON_SO)

$(PYTHON_SO): pjsua2_wrap.cpp setup.py $(GCC_EXE)
	$(PYTHON_EXE) setup.py build $(PYTHON_SETUP_FLAGS)

gcc.exe: cc_mingw.c
	gcc -o gcc.exe cc_mingw.c
	cp gcc.exe g++.exe

pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	swig $(SWIG_FLAGS) -python -o pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(PYTHON_SO) pjsua2_wrap.cpp pjsua2_wrap.h pjsua2.py build *.pyc
	rm -f gcc.exe g++.exe

install:
	$(PYTHON_EXE) setup.py install --user

uninstall:
	rm -f $(PYTHON_PKG_DIR)/pjsua2*
	rm -f $(PYTHON_PKG_DIR)/_pjsua2*

