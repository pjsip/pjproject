RUBY_SO=pjsua2.so

SWIG_FLAGS=-I../../../../pjlib/include \
		   -I../../../../pjlib-util/include \
		   -I../../../../pjmedia/include \
		   -I../../../../pjsip/include \
		   -I../../../../pjnath/include -c++
SRC_DIR=../../../../pjsip/include
SRCS=$(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

SWIG_FLAGS += -w312 $(USE_THREADS)

.PHONY: all install uninstall

all: $(RUBY_SO)

$(RUBY_SO): pjsua2_wrap.o
	g++ -shared -o $(RUBY_SO) $(LDFLAGS) pjsua2_wrap.o -L/usr/local/lib -lpjsua2

pjsua2_wrap.o: pjsua2_wrap.cpp
	g++ $(CXXFLAGS) -fPIC -c pjsua2_wrap.cpp

pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	swig $(SWIG_FLAGS) -ruby -o pjsua2_wrap.cpp ../pjsua2.i

clean distclean realclean:
	rm -rf $(RUBY_SO) pjsua2_wrap.cpp pjsua2_wrap.o pjsua2_wrap.h

install:
	cp $(RUBY_SO) /opt/rh/rh-ruby26/root/usr/lib64/ruby/
