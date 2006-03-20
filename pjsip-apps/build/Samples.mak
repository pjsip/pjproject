
include ../../build/common.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSIP_LIB:=../../pjsip/lib/libpjsip-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSIP_UA_LIB:=../../pjsip/lib/libpjsip-ua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSIP_SIMPLE_LIB:=../../pjsip/lib/libpjsip-simple-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSUA_LIB_LIB=../../pjsip/lib/libpjsua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../../pjsip/include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)

export LIBS :=	$(subst /,$(HOST_PSEP),$(PJSUA_LIB_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_UA_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_SIMPLE_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJSIP_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_CODEC_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		$(subst /,$(HOST_PSEP),$(PJLIB_LIB))


export _LDFLAGS := $(LIBS) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) -lm

SRCDIR := ../src/samples
OBJDIR := ./output/samples-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)
BINDIR := ../bin/samples

CFLAGS = $(_CFLAGS)
LDFLAGS = $(_LDFLAGS)

SAMPLES := simpleua playfile playsine confsample sndinfo level

EXES := $(foreach file, $(SAMPLES), $(BINDIR)/$(file)-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE))

all: $(OBJDIR) $(EXES)

$(BINDIR)/%-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(HOST_EXE): $(OBJDIR)/%$(OBJEXT) $(LIBS)
	$(LD) $(LDOUT)$(subst /,$(HOST_PSEP),$@) \
	    $(subst /,$(HOST_PSEP),$<) \
	    $(_LDFLAGS)

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.c
	$(CC) $(CFLAGS) \
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

