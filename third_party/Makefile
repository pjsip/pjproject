include ../build.mak
include $(PJDIR)/build/host-$(HOST_NAME).mak

DIRS = build/resample

ifeq (,1)
else
DIRS += build/gsm
endif

ifeq (,1)
else
DIRS += build/ilbc
endif

ifeq (,1)
else
DIRS += build/speex
endif

ifeq (pa_unix,pa_old_darwinos)
DIRS += build/portaudio
endif

ifeq (pa_unix,pa_darwinos)
DIRS += build/portaudio
endif

ifeq (pa_unix,pa_win32)
DIRS += build/portaudio
endif

ifeq (pa_unix,pa_unix)
DIRS += build/portaudio
endif



all clean dep depend distclean realclean:
	for dir in $(DIRS); do \
	   if [ -d $$dir ]; then \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	   fi; \
	done

