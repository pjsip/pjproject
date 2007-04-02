
include ../../build/common.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=../../pjnath/lib/libpjnath-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)
PJSIP_LIB:=../../pjsip/lib/libpjsip-$(TARGET_NAME)$(LIBEXT)
PJSIP_UA_LIB:=../../pjsip/lib/libpjsip-ua-$(TARGET_NAME)$(LIBEXT)
PJSIP_SIMPLE_LIB:=../../pjsip/lib/libpjsip-simple-$(TARGET_NAME)$(LIBEXT)
PJSUA_LIB_LIB=../../pjsip/lib/libpjsua-$(TARGET_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../../pjsip/include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjnath/include \
		   $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)

export LIBS :=	$(subst /,$(HOST_PSEP),$(PJSUA_LIB_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_UA_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_SIMPLE_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_CODEC_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_CODEC_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJNATH_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJLIB_LIB))


export _LDFLAGS := $(LIBS) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) -lm

SRCDIR := ../src/samples
OBJDIR := ./output/samples-$(TARGET_NAME)
BINDIR := ../bin/samples

SAMPLES := confsample \
	   level \
	   pjsip-perf \
	   playfile \
	   playsine \
	   recfile \
	   resampleplay \
	   simpleua \
	   simple_pjsua \
	   siprtp \
	   sipstateless \
	   sndinfo \
	   sndtest \
	   stateful_proxy \
	   stateless_proxy \
	   streamutil \
	   tonegen

EXES := $(foreach file, $(SAMPLES), $(BINDIR)/$(file)-$(TARGET_NAME)$(HOST_EXE))

all: $(OBJDIR) $(EXES)

$(BINDIR)/%-$(TARGET_NAME)$(HOST_EXE): $(OBJDIR)/%$(OBJEXT) $(LIBS)
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

