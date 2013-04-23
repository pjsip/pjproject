#
# This is a utility make file that is used to generate BB10 config settings
# from current build settings. The stdout output of this make file can be
# saved to a .pri file which then can be included in app's .pro file.
#
# This make file is invoked by configure-bb10 script to generate pjsip.pri
# file.
#
include build.mak

# Generate library list (the "-lxxx" options) from list of linked libraries.
PJ_BB_LIBS = $(filter-out -lm -lsocket, $(APP_LDLIBS))

# This used to generate the library path list (the "-Lxxx" options)
# We replace the path with "$$PJ_DIR"
PJ_BB_LDFLAGS = $(subst $(PJDIR),\$$\$$PJ_DIR,$(APP_LDFLAGS))

all: 
	@echo PJ_DIR = $(PJDIR)
	@echo
	@echo 'DEFINES += PJ_AUTOCONF'
	@echo
	@echo 'PJ_INCLUDEPATH += $$$$quote($$$$PJ_DIR/pjlib/include)'
	@echo 'PJ_INCLUDEPATH += $$$$quote($$$$PJ_DIR/pjmedia/include)'
	@echo 'PJ_INCLUDEPATH += $$$$quote($$$$PJ_DIR/pjnath/include)'
	@echo 'PJ_INCLUDEPATH += $$$$quote($$$$PJ_DIR/pjlib-util/include)'
	@echo 'PJ_INCLUDEPATH += $$$$quote($$$$PJ_DIR/pjsip/include)'
	@echo
	@for token in $(PJ_BB_LDFLAGS); do \
		if echo $$token | grep -- '-L' >> /dev/null; then \
			echo "PJ_LIBPATH += \$$\$$quote($$token)"; \
		fi; \
	done
	@echo
	@for token in $(PJ_BB_LIBS); do \
		echo PJ_LIBS += $$token; \
	done
	@echo
	@echo 'INCLUDEPATH += $$$$PJ_INCLUDEPATH'
	@echo 'LIBS += $$$$PJ_LIBPATH'
	@echo 'LIBS += $$$$PJ_LIBS'
	@echo 'LIBS += -lOpenAL -lalut -laudio_manager -lsocket -lasound -lbbsystem -lm'


