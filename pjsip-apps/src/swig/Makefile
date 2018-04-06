include ../../../build.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
    # no python for android
    DIRS = java csharp
else
    ifneq ($(findstring ios,$(TARGET_NAME)),)
  	DIRS = csharp
    else
  	DIRS = python java
    endif
endif

export SWIG_FLAGS=-I../../../../pjlib/include \
		   -I../../../../pjlib-util/include \
		   -I../../../../pjmedia/include \
		   -I../../../../pjsip/include \
		   -I../../../../pjnath/include -c++ 
export SRC_DIR=../../../../pjsip/include
export SRCS=$(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

.PHONY: all clean dep depend distclean print realclean install uninstall

all: symbols.i

all clean dep depend distclean print realclean install uninstall:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done

csharp:
	$(MAKE) $(MAKE_FLAGS) -C csharp $@

symbols.i: symbols.lst
	@echo warning: file symbols.i is out of date. Run 'make symbol'.

symbol symbols:
	python importsym.py

