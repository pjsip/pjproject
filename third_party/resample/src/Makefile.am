## Process this file with automake to produce Makefile.in

SNDLIB = ../sndlib-20

INCLUDES = -I$(SNDLIB)

bin_PROGRAMS = resample windowfilter

resample_SOURCES = resample.c resamplesubs.c sndlibextra.c filterkit.c \
	$(SNDLIB)/audio.c $(SNDLIB)/io.c $(SNDLIB)/headers.c $(SNDLIB)/sound.c \
	resample.h stdefs.h sndlibextra.h filterkit.h smallfilter.h largefilter.h \
	$(SNDLIB)/sndlib.h $(SNDLIB)/mus-config.h $(SNDLIB)/_sndlib.h $(SNDLIB)/sndlib-strings.h  $(SNDLIB)/configure

resample_LDADD = -lm

windowfilter_SOURCES = windowfilter.c resamplesubs.c sndlibextra.c filterkit.c \
	$(SNDLIB)/audio.c $(SNDLIB)/io.c $(SNDLIB)/headers.c $(SNDLIB)/sound.c

windowfilter_LDADD = -lm

EXTRA_DIST = $(man_MANS)

