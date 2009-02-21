
include ../../build/common.mak


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(PJ_CFLAGS) $(CFLAGS)
export _CXXFLAGS:= $(PJ_CXXFLAGS)
export _LDFLAGS := $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS)

SRCDIR := ../src/samples
OBJDIR := ./output/samples-$(TARGET_NAME)
BINDIR := ../bin/samples

SAMPLES := auddemo \
	   confsample \
	   encdec \
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
	   siprtp \
	   sipstateless \
	   stateful_proxy \
	   stateless_proxy \
	   stereotest \
	   streamutil \
	   strerror \
	   tonegen

EXES := $(foreach file, $(SAMPLES), $(BINDIR)/$(file)-$(TARGET_NAME)$(HOST_EXE))

all: $(OBJDIR) $(EXES)

$(BINDIR)/%-$(TARGET_NAME)$(HOST_EXE): $(OBJDIR)/%$(OBJEXT) $(PJ_LIB_FILES)
	$(LD) $(LDOUT)$(subst /,$(HOST_PSEP),$@) \
	    $(subst /,$(HOST_PSEP),$<) \
	    $(_LDFLAGS)

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.c
	$(CC) $(_CFLAGS) \
	  $(CC_OUT)$(subst /,$(HOST_PSEP),$@) \
	  $(subst /,$(HOST_PSEP),$<) 

$(OBJDIR):
	$(subst @@,$(subst /,$(HOST_PSEP),$@),$(HOST_MKDIR)) 

depend:

clean:
	$(subst @@,$(subst /,$(HOST_PSEP),$(OBJDIR)/*),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),$(OBJDIR)),$(HOST_RMDIR))
	$(subst @@,$(EXES),$(HOST_RM))

distclean realclean: clean
#	$(subst @@,$(subst /,$(HOST_PSEP),$(EXES)) $(subst /,$(HOST_PSEP),$(EXES)),$(HOST_RM))
#	$(subst @@,$(DEP_FILE),$(HOST_RM))

