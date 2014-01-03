include ../../build.mak
include ../../version.mak

THIRD_PARTY:=$(PJDIR)/third_party

include $(PJDIR)/build/common.mak

export LIBDIR := ../lib
export BINDIR := ../bin

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=$(PJDIR)/pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=$(PJDIR)/pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=$(PJDIR)/pjnath/lib/libpjnath-$(TARGET_NAME)$(LIBEXT)

export PJMEDIA_LIB:=libpjmedia-$(TARGET_NAME)$(LIBEXT)
export PJMEDIA_CODEC_LIB:=libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)
export PJSDP_LIB:=libpjsdp-$(TARGET_NAME)$(LIBEXT)
export PJMEDIA_AUDIODEV_LIB:=libpjmedia-audiodev-$(TARGET_NAME)$(LIBEXT)
export PJMEDIA_VIDEODEV_LIB:=libpjmedia-videodev-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export PJMEDIA_SONAME:=libpjmedia.$(SHLIB_SUFFIX)
export PJMEDIA_SHLIB:=$(PJMEDIA_SONAME).$(PJ_VERSION_MAJOR)
export PJMEDIA_CODEC_SONAME:=libpjmedia-codec.$(SHLIB_SUFFIX)
export PJMEDIA_CODEC_SHLIB:=$(PJMEDIA_CODEC_SONAME).$(PJ_VERSION_MAJOR)
export PJSDP_SONAME:=libpjsdp.$(SHLIB_SUFFIX)
export PJSDP_SHLIB:=$(PJSDP_SONAME).$(PJ_VERSION_MAJOR)
export PJMEDIA_AUDIODEV_SONAME:=libpjmedia-audiodev.$(SHLIB_SUFFIX)
export PJMEDIA_AUDIODEV_SHLIB:=$(PJMEDIA_AUDIODEV_SONAME).$(PJ_VERSION_MAJOR)
export PJMEDIA_VIDEODEV_SONAME:=libpjmedia-videodev.$(SHLIB_SUFFIX)
export PJMEDIA_VIDEODEV_SHLIB:=$(PJMEDIA_VIDEODEV_SONAME).$(PJ_VERSION_MAJOR)
endif


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjmedia/include \
		   $(CC_INC)../../pjnath/include \
		   $(CC_INC)../..
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)

export _LDFLAGS := $(APP_THIRD_PARTY_LIBS) \
		   $(APP_THIRD_PARTY_EXT) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS) 

###############################################################################
# Defines for building PJMEDIA library
#
export PJMEDIA_SRCDIR = ../src/pjmedia
export PJMEDIA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			alaw_ulaw.o alaw_ulaw_table.o avi_player.o \
			bidirectional.o clock_thread.o codec.o conference.o \
			conf_switch.o converter.o  converter_libswscale.o \
			delaybuf.o echo_common.o \
			echo_port.o echo_suppress.o endpoint.o errno.o \
			event.o format.o ffmpeg_util.o \
			g711.o jbuf.o master_port.o mem_capture.o mem_player.o \
			null_port.o plc_common.o port.o splitcomb.o \
			resample_resample.o resample_libsamplerate.o resample_speex.o \
			resample_port.o rtcp.o rtcp_xr.o rtp.o \
			sdp.o sdp_cmp.o sdp_neg.o session.o silencedet.o \
			sound_legacy.o sound_port.o stereo_port.o stream_common.o \
			stream.o stream_info.o tonegen.o transport_adapter_sample.o \
			transport_ice.o transport_loop.o transport_srtp.o transport_udp.o \
			types.o vid_codec.o vid_codec_util.o \
			vid_port.o vid_stream.o vid_stream_info.o vid_tee.o \
			wav_player.o wav_playlist.o wav_writer.o wave.o \
			wsola.o

export PJMEDIA_CFLAGS += $(_CFLAGS)
export PJMEDIA_CXXFLAGS += $(_CXXFLAGS)
export PJMEDIA_LDFLAGS += $(PJMEDIA_VIDEODEV_LDLIB) \
			  $(PJMEDIA_AUDIODEV_LDLIB) \
			  $(PJLIB_LDLIB) \
			  $(PJLIB_UTIL_LDLIB) \
			  $(PJNATH_LDLIB) \
			  $(_LDFLAGS)


###############################################################################
# Defines for building PJMEDIA-AUDIODEV library
#
export PJMEDIA_AUDIODEV_SRCDIR = ../src/pjmedia-audiodev
export PJMEDIA_AUDIODEV_OBJS +=  audiodev.o audiotest.o errno.o \
				 legacy_dev.o null_dev.o pa_dev.o wmme_dev.o \
				 alsa_dev.o bb10_dev.o bdimad_dev.o \
				 android_jni_dev.o opensl_dev.o
export PJMEDIA_AUDIODEV_CFLAGS += $(_CFLAGS)
export PJMEDIA_AUDIODEV_CXXFLAGS += $(_CXXFLAGS)
export PJMEDIA_AUDIODEV_LDFLAGS += $(PJLIB_LDLIB) \
				   $(_LDFLAGS)


###############################################################################
# Defines for building PJMEDIA-VIDEODEV library
#
export PJMEDIA_VIDEODEV_SRCDIR = ../src/pjmedia-videodev
export PJMEDIA_VIDEODEV_OBJS +=  errno.o videodev.o avi_dev.o ffmpeg_dev.o \
				colorbar_dev.o v4l2_dev.o
export PJMEDIA_VIDEODEV_CFLAGS += $(_CFLAGS)
export PJMEDIA_VIDEODEV_CXXFLAGS += $(_CXXFLAGS)
export PJMEDIA_VIDEODEV_LDFLAGS += $(PJLIB_LDLIB) \
				   $(_LDFLAGS)


###############################################################################
# Defines for building PJSDP library
# Note that SDP functionality is already INCLUDED in PJMEDIA.
# The PJSDP library should only be used for applications that want SDP
# but don't want to use the rest of the media framework.
#
export PJSDP_SRCDIR = ../src/pjmedia
export PJSDP_OBJS += 	$(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			errno.o sdp.o sdp_cmp.o sdp_neg.o
export PJSDP_CFLAGS += $(_CFLAGS)
export PJSDP_CXXFLAGS += $(_CXXFLAGS)
export PJSDP_LDFLAGS += $(PJMEDIA_LDLIB) \
			$(PJLIB_LDLIB) \
			$(PJLIB_UTIL_LDLIB) \
			$(_LDFLAGS)


###############################################################################
# Defines for building PJMEDIA-Codec library
#
export PJMEDIA_CODEC_SRCDIR = ../src/pjmedia-codec
export PJMEDIA_CODEC_OBJS += audio_codecs.o ffmpeg_vid_codecs.o \
			h263_packetizer.o h264_packetizer.o \
			$(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			ipp_codecs.o opencore_amr.o silk.o $(CODEC_OBJS) \
                        g7221_sdp_match.o amr_sdp_match.o
export PJMEDIA_CODEC_CFLAGS += $(_CFLAGS) $(GSM_CFLAGS) $(SPEEX_CFLAGS) \
			$(ILBC_CFLAGS) $(IPP_CFLAGS) $(G7221_CFLAGS)
export PJMEDIA_CODEC_CXXFLAGS += $(_CXXFLAGS) $(GSM_CFLAGS) $(SPEEX_CFLAGS) \
			$(ILBC_CFLAGS) $(IPP_CFLAGS) $(G7221_CFLAGS)
export PJMEDIA_CODEC_LDFLAGS += $(PJMEDIA_LDLIB) \
				$(PJLIB_LDLIB) \
				$(_LDFLAGS)

###############################################################################
# Defines for building test application
#
export PJMEDIA_TEST_SRCDIR = ../src/test
export PJMEDIA_TEST_OBJS += codec_vectors.o jbuf_test.o main.o mips_test.o \
			    vid_codec_test.o vid_dev_test.o vid_port_test.o \
			    rtp_test.o test.o
export PJMEDIA_TEST_OBJS += sdp_neg_test.o 
export PJMEDIA_TEST_CFLAGS += $(_CFLAGS)
export PJMEDIA_TEST_CXXFLAGS += $(_CXXFLAGS)
export PJMEDIA_TEST_LDFLAGS += $(PJMEDIA_CODEC_LDLIB) \
			       $(PJMEDIA_VIDEODEV_LDLIB) \
			       $(PJMEDIA_LDLIB) \
			       $(PJMEDIA_AUDIODEV_LDLIB) \
			       $(PJLIB_LDLIB) \
			       $(PJLIB_UTIL_LDLIB) \
			       $(PJNATH_LDLIB) \
			       $(_LDFLAGS)
export PJMEDIA_TEST_EXE:=pjmedia-test-$(TARGET_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(PJMEDIA_LIB) $(PJMEDIA_SONAME) \
	   $(PJMEDIA_CODEC_LIB) $(PJMEDIA_CODEC_SONAME)\
	   $(PJMEDIA_VIDEODEV_LIB) $(PJMEDIA_VIDEODEV_SONAME) \
	   $(PJMEDIA_AUDIODEV_LIB) $(PJMEDIA_AUDIODEV_SONAME) \
	   $(PJSDP_LIB) $(PJSDP_SONAME) \
	   $(PJMEDIA_TEST_EXE)

all: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjmedia/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(PJMEDIA_LIB) $(PJMEDIA_SONAME)
.PHONY: $(PJMEDIA_CODEC_LIB) $(PJMEDIA_CODEC_SONAME)
.PHONY: $(PJMEDIA_VIDEODEV_LIB) $(PJMEDIA_VIDEODEV_SONAME)
.PHONY: $(PJMEDIA_AUDIODEV_LIB) $(PJMEDIA_AUDIODEV_SONAME)
.PHONY: $(PJSDP_LIB) $(PJSDP_SONAME)
.PHONY: $(PJMEDIA_TEST_EXE)

pjmedia: $(PJMEDIA_LIB)
$(PJMEDIA_SONAME): $(PJMEDIA_LIB)
$(PJMEDIA_LIB) $(PJMEDIA_SONAME): $(PJMEDIA_AUDIODEV_LIB) $(PJMEDIA_AUDIODEV_SONAME) $(PJMEDIA_VIDEODEV_LIB) $(PJMEDIA_VIDEODEV_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjmedia-codec: $(PJMEDIA_CODEC_LIB)
$(PJMEDIA_CODEC_SONAME): $(PJMEDIA_CODEC_LIB)
$(PJMEDIA_CODEC_LIB) $(PJMEDIA_CODEC_SONAME): $(PJMEDIA_LIB) $(PJMEDIA_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjmedia-videodev: $(PJMEDIA_VIDEODEV_LIB)
$(PJMEDIA_VIDEODEV_SONAME): $(PJMEDIA_VIDEODEV_LIB)
$(PJMEDIA_VIDEODEV_LIB) $(PJMEDIA_VIDEODEV_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_VIDEODEV app=pjmedia-videodev $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjmedia-audiodev: $(PJMEDIA_AUDIODEV_LIB)
$(PJMEDIA_AUDIODEV_SONAME): $(PJMEDIA_AUDIODEV_LIB)
$(PJMEDIA_AUDIODEV_LIB) $(PJMEDIA_AUDIODEV_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

pjsdp: $(PJSDP_LIB)
$(PJSDP_SONAME): $(PJSDP_LIB)
$(PJSDP_LIB) $(PJSDP_SONAME): $(PJMEDIA_LIB) $(PJMEDIA_SONAME)
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $(LIBDIR)/$@

pjmedia-test: $(PJMEDIA_TEST_EXE)
$(PJMEDIA_TEST_EXE): $(PJMEDIA_LIB) $(PJMEDIA_SONAME)
$(PJMEDIA_TEST_EXE): $(PJMEDIA_AUDIODEV_LIB) $(PJMEDIA_AUDIODEV_SONAME)
$(PJMEDIA_TEST_EXE): $(PJMEDIA_VIDEODEV_LIB) $(PJMEDIA_VIDEODEV_SONAME)
$(PJMEDIA_TEST_EXE): $(PJMEDIA_CODEC_LIB) $(PJMEDIA_CODEC_SONAME)
$(PJMEDIA_TEST_EXE):
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $(BINDIR)/$@

.PHONY: pjmedia.ko
pjmedia.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $(LIBDIR)/$@

.PHONY: pjmedia-codec.ko
pjmedia-codec.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $(LIBDIR)/$@

.PHONY: pjmedia-test.ko
pjmedia-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $(LIBDIR)/$@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_VIDEODEV app=pjmedia-videodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-videodev-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-audiodev-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-codec-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsdp-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_VIDEODEV app=pjmedia-videodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_VIDEODEV app=pjmedia-videodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $@
	echo '$(BINDIR)/$(PJMEDIA_TEST_EXE): $(LIBDIR)/$(PJMEDIA_LIB) $(LIBDIR)/$(PJMEDIA_CODEC_LIB) $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjmedia-test-$(TARGET_NAME).depend


