PYTHON_SO=_pjsua2.so

PYTHON_SETUP_FLAGS = --inplace 
ifeq ($(OS),Windows_NT)
  PYTHON_SETUP_FLAGS += --compiler=mingw32
endif

.PHONY: all

all: $(PYTHON_SO)

$(PYTHON_SO): pjsua2_wrap.cpp setup.py
	python setup.py build_ext $(PYTHON_SETUP_FLAGS)

pjsua2_wrap.cpp: ../pjsua2.i $(SRCS)
	swig $(SWIG_FLAGS) -python  -o pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(PYTHON_SO) pjsua2_wrap.cpp pjsua2_wrap.h pjsua2.py build *.pyc
