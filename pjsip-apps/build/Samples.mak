include ../../build.mak
include ../../version.mak
include ../../build/common.mak

RULES_MAK := $(PJDIR)/build/rules.mak

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(PJ_CFLAGS) $(CFLAGS)
export _CXXFLAGS:= $(PJ_CXXFLAGS)
export _LDFLAGS := $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS)

SRCDIR := ../src/samples
OBJDIR := ./output/samples-$(TARGET_NAME)
BINDIR := ../bin/samples/$(TARGET_NAME)

SAMPLES := auddemo \
	   aviplay \
	   aectest \
	   clidemo \
	   confsample \
	   encdec \
	   httpdemo \
	   icedemo \
	   jbsim \
	   latency \
	   level \
	   mix \
	   pjsip-perf \
	   pcaputil \
	   playfile \
	   playsine \
	   recfile \
	   resampleplay \
	   simpleua \
	   simple_pjsua \
	   sipecho \
	   siprtp \
	   sipstateless \
	   stateful_proxy \
	   stateless_proxy \
	   stereotest \
	   streamutil \
	   strerror \
	   tonegen \
	   vid_streamutil

EXES := $(foreach file, $(SAMPLES), $(file)$(HOST_EXE))

.PHONY: $(EXES)

all: $(EXES)

$(EXES):
	$(MAKE) --no-print-directory -f $(RULES_MAK) SAMPLE_SRCDIR=$(SRCDIR) SAMPLE_OBJS=$@.o SAMPLE_CFLAGS="$(_CFLAGS)" SAMPLE_LDFLAGS="$(_LDFLAGS)" SAMPLE_EXE=$@ APP=SAMPLE app=sample $(subst /,$(HOST_PSEP),$(BINDIR)/$@)

depend:

clean:
	$(MAKE) -f $(RULES_MAK) APP=SAMPLE app=sample $@
	$(subst @@,$(EXES),$(HOST_RM))
	$(subst @@,$(BINDIR),$(HOST_RMDIR))

distclean realclean: clean
	$(MAKE) -f $(RULES_MAK) APP=SAMPLE app=sample $@

