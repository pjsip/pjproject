DIRS =

include ../../build.mak
include $(PJDIR)/build/common.mak

all clean dep depend distclean realclean:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done

lib:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir all; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done

doc:
	
