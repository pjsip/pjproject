PYTHON_SO=_pjsua2.so

USE_PYTHON3=1

ifeq ($(USE_PYTHON3),1)
  PYTHON_EXE=python3
  PYTHON_PKG_DIR=$(HOME)/.local/lib/python3.6/site-packages
else
  PYTHON_EXE=python
  PYTHON_PKG_DIR=$(HOME)/.local/lib/python2.7/site-packages
endif

#PYTHON_SETUP_FLAGS = --inplace 
ifeq ($(OS),Windows_NT)
  PYTHON_SETUP_FLAGS += --compiler=mingw32
  GCC_EXE=gcc.exe
else
  GCC_EXE=
endif

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

