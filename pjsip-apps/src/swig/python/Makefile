PYTHON_SO=_pjsua2.so

#PYTHON_SETUP_FLAGS = --inplace 
ifeq ($(OS),Windows_NT)
  PYTHON_SETUP_FLAGS += --compiler=mingw32
endif

SWIG_FLAGS += -w312

.PHONY: all install uninstall

all: $(PYTHON_SO)

$(PYTHON_SO): pjsua2_wrap.cpp setup.py
	python setup.py build $(PYTHON_SETUP_FLAGS)

pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	swig $(SWIG_FLAGS) -python  -o pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(PYTHON_SO) pjsua2_wrap.cpp pjsua2_wrap.h pjsua2.py build *.pyc

install:
	python setup.py install --user

uninstall:
	rm -f $(HOME)/.local/lib/python2.7/site-packages/pjsua2*
	rm -f $(HOME)/.local/lib/python2.7/site-packages/_pjsua2*

