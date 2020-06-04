include build.mak
include build/host-$(HOST_NAME).mak
-include user.mak
include version.mak

LIB_DIRS = pjlib/build pjlib-util/build pjnath/build third_party/build pjmedia/build pjsip/build
DIRS = $(LIB_DIRS) pjsip-apps/build $(EXTRA_DIRS)

ifdef MINSIZE
MAKE_FLAGS := MINSIZE=1
endif

all clean dep depend print:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done

distclean realclean:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done
	$(HOST_RM) config.log
	$(HOST_RM) config.status

lib:
	for dir in $(LIB_DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir lib; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done; \


.PHONY: lib doc

doc:
	@if test \( ! "$(WWWDIR)" == "" \) -a \( ! -d $(WWWDIR)/pjlib/docs/html \) ; then \
		echo 'Directory "$(WWWDIR)" does not look like a valid pjsip web directory'; \
		exit 1; \
	fi
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done
	
LIBS = 	pjlib/lib/libpj-$(TARGET_NAME).a \
	pjlib-util/lib/libpjlib-util-$(TARGET_NAME).a \
	pjnath/lib/libpjnath-$(TARGET_NAME).a \
	pjmedia/lib/libpjmedia-$(TARGET_NAME).a \
	pjmedia/lib/libpjmedia-audiodev-$(TARGET_NAME).a \
	pjmedia/lib/libpjmedia-codec-$(TARGET_NAME).a \
    	pjsip/lib/libpjsip-$(TARGET_NAME).a \
	pjsip/lib/libpjsip-ua-$(TARGET_NAME).a \
	pjsip/lib/libpjsip-simple-$(TARGET_NAME).a \
	pjsip/lib/libpjsua-$(TARGET_NAME).a
BINS = 	pjsip-apps/bin/pjsua-$(TARGET_NAME)$(HOST_EXE) 

size:
	@echo -n 'Date: '
	@date
	@echo
	@for lib in $(LIBS); do \
		echo "$$lib:"; \
		size -t $$lib | awk '{print $$1 "\t" $$2 "\t" $$3 "\t" $$6}'; \
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

selftest: pjlib-test pjlib-util-test pjnath-test pjmedia-test pjsip-test pjsua-test

pjlib-test: pjlib/bin/pjlib-test-$(TARGET_NAME)
	cd pjlib/build && ../bin/pjlib-test-$(TARGET_NAME)

pjlib-util-test: pjlib-util/bin/pjlib-util-test-$(TARGET_NAME)
	cd pjlib-util/build && ../bin/pjlib-util-test-$(TARGET_NAME)

pjnath-test: pjnath/bin/pjnath-test-$(TARGET_NAME)
	cd pjnath/build && ../bin/pjnath-test-$(TARGET_NAME)

pjmedia-test: pjmedia/bin/pjmedia-test-$(TARGET_NAME)
	cd pjmedia/build && ../bin/pjmedia-test-$(TARGET_NAME)

pjsip-test: pjsip/bin/pjsip-test-$(TARGET_NAME)
	cd pjsip/build && ../bin/pjsip-test-$(TARGET_NAME)

pjsua-test: cmp_wav
	cd tests/pjsua && python runall.py

cmp_wav:
	cd tests/pjsua/tools && make

install:
	mkdir -p $(DESTDIR)$(libdir)/
#	cp -af $(APP_LIB_FILES) $(DESTDIR)$(libdir)/
	cp -af $(APP_LIBXX_FILES) $(DESTDIR)$(libdir)/
	mkdir -p $(DESTDIR)$(includedir)/
	for d in pjlib pjlib-util pjnath pjmedia pjsip; do \
		cp -RLf $$d/include/* $(DESTDIR)$(includedir)/; \
	done
	mkdir -p $(DESTDIR)$(libdir)/pkgconfig
	sed -e "s!@PREFIX@!$(prefix)!" libpjproject.pc.in | \
		sed -e "s!@INCLUDEDIR@!$(includedir)!" | \
		sed -e "s!@LIBDIR@!$(libdir)!" | \
		sed -e "s/@PJ_VERSION@/$(PJ_VERSION)/" | \
		sed -e "s!@PJ_INSTALL_LDFLAGS@!$(PJ_INSTALL_LDFLAGS)!" | \
		sed -e "s!@PJ_INSTALL_CFLAGS@!$(PJ_INSTALL_CFLAGS)!" > $(DESTDIR)/$(libdir)/pkgconfig/libpjproject.pc

uninstall:
	$(RM) $(DESTDIR)$(libdir)/pkgconfig/libpjproject.pc
	rmdir $(DESTDIR)$(libdir)/pkgconfig 2> /dev/null || true
	for d in pjlib pjlib-util pjnath pjmedia pjsip; do \
		for f in $$d/include/*; do \
			$(RM) -r "$(DESTDIR)$(includedir)/`basename $$f`"; \
		done; \
	done
	rmdir $(DESTDIR)$(includedir) 2> /dev/null || true
	$(RM) $(addprefix $(DESTDIR)$(libdir)/,$(notdir $(APP_LIB_FILES)))
	rmdir $(DESTDIR)$(libdir) 2> /dev/null || true
