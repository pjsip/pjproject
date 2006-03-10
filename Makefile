include build.mak
include build/host-$(HOST_NAME).mak

DIRS = pjlib pjlib-util pjmedia pjsip pjsip-apps

ifdef MINSIZE
MAKE_FLAGS := MINSIZE=1
endif

all clean dep depend distclean doc print realclean:
	for dir in $(DIRS); do \
	   if [ -d $$dir ]; then \
		if make $(MAKE_FLAGS) -C $$dir/build $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	   fi; \
	done

LIBS = 	pjlib/lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
	pjlib-util/lib/libpjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
	pjmedia/lib/libpjmedia-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
	pjmedia/lib/libpjmedia-codec-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
       	pjsip/lib/libpjsip-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
	pjsip/lib/libpjsip-ua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
	pjsip/lib/libpjsip-simple-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a \
	pjsip/lib/libpjsua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME).a
BINS = 	pjsip-apps/bin/pjsua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE) 

size:
	@echo -n 'Date: '
	@date
	@echo
	@for lib in $(LIBS); do \
		echo "$$lib:"; \
		ar tv $$lib | awk '{print $$3 "\t" $$8}' | sort -n; \
		echo -n 'Total: '; \
		ar tv $$lib | awk '{print " + " $$3}' | xargs expr 0; \
		echo; \
	done
	@echo
	@for bin in $(BINS); do \
		echo "size $$bin:"; \
		size $$bin; \
	done

#dos2unix:
#	for f in `find . | egrep '(mak|h|c|S|s|Makefile)$$'`; do \
#		dos2unix "$$f" > dos2unix.tmp; \
#		cp dos2unix.tmp "$$f"; \
#	done
#	rm -f dos2unix.tmp

xhdrid:
	for f in `find . | egrep '\.(h|c|S|s|cpp|hpp)$$'`; do \
		echo Processing $$f...; \
		cat $$f | sed 's/.*\$$Author\$$/ */' > /tmp/id; \
		cp /tmp/id $$f; \
	done

