PYTHON_SO=_pjsua2.so

.PHONY: all

all: $(PYTHON_SO)

$(PYTHON_SO): pjsua2_wrap.cpp setup.py
	python setup.py build_ext --inplace

pjsua2_wrap.cpp: ../pjsua2.i $(SRCS)
	swig $(SWIG_FLAGS) -python  -o pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(PYTHON_SO) pjsua2_wrap.cpp pjsua2_wrap.h pjsua2.py build *.pyc
