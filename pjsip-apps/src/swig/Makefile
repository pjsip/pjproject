DIRS = python java

export SWIG_FLAGS=-I../../../../pjlib/include \
		   -I../../../../pjlib-util/include \
		   -I../../../../pjmedia/include \
		   -I../../../../pjsip/include \
		   -I../../../../pjnath/include -c++ 
export SRC_DIR=../../../../pjsip/include
export SRCS=$(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

all clean dep depend distclean print realclean:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done
