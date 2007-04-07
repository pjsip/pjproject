/* Audio hardware handlers (SGI, OSS, ALSA, Sun, Windows, Mac OSX, Jack, ESD, HPUX, NetBSD) */

/*
 * layout of this file:
 *    error handlers
 *    SGI new and old audio library
 *    OSS (with Sam 9407 support)
 *    ALSA
 *    Sun (has switches for OpenBSD, but they're untested)
 *    Windows 95/98
 *    OSX
 *    ESD
 *    JACK
 *    HPUX
 *    NetBSD
 *    audio describers
 */

/*
 * void mus_audio_describe(void) describes the audio hardware state.
 * char *mus_audio_report(void) returns the same information as a string.
 *
 * int mus_audio_open_output(int dev, int srate, int chans, int format, int size)
 * int mus_audio_open_input(int dev, int srate, int chans, int format, int size)
 * int mus_audio_write(int line, char *buf, int bytes)
 * int mus_audio_close(int line)
 * int mus_audio_read(int line, char *buf, int bytes)
 *
 * int mus_audio_mixer_read(int dev, int field, int chan, float *val)
 * int mus_audio_mixer_write(int dev, int field, int chan, float *val)
 * int mus_audio_initialize(void) does whatever is needed to get set up
 * int mus_audio_systems(void) returns number of separate complete audio systems (soundcards essentially)
 * AUDIO_SYSTEM(n) selects the nth card (counting from 0), AUDIO_SYSTEM(0) is always the default
 * char *mus_audio_system_name(int system) returns some user-recognizable (?) name for the given card (don't free)
 * char *mus_audio_moniker(void) returns some brief description of the overall audio setup (don't free return string).
 */

/* error handling is tricky here -- higher levels are using many calls as probes, so
 *   the "error" is a sign of non-existence, not a true error.  So, for nearly all
 *   cases, I'll use mus_print, not mus_error.
 */

#include <mus-config.h>

#if USE_SND && MUS_MAC_OSX && USE_MOTIF
  #undef USE_MOTIF
  #define USE_NO_GUI 1
  /* Xt's Boolean (/usr/include/X11/Intrinsic.h = char) collides with MacTypes.h Boolean, (actually,
   *   unsigned char in /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/CoreFoundation.framework/Versions/A/Headers/CFBase.h)
   *   but we want snd.h for other stuff, so, if Motif is in use, don't load its headers at this time
   *   perhaps we could use the -funsigned-char switch in gcc
   */
#endif

#if USE_SND && MUS_MAC_OSX && HAVE_RUBY
  /* if using Ruby, OpenTransport.h T_* definitions collide with Ruby's -- it isn't needed here, so... */
  #define REDEFINE_HAVE_RUBY 1
  #undef HAVE_RUBY
#endif

#if USE_SND
  #include "snd.h"
#else
  #define PRINT_BUFFER_SIZE 512
  #define LABEL_BUFFER_SIZE 64
#endif

#if USE_SND && MUS_MAC_OSX
  #define USE_MOTIF 1
  #undef USE_NO_GUI
  #if REDEFINE_HAVE_RUBY
    #define HAVE_RUBY 1
  #endif
#endif

#include <math.h>
#include <stdio.h>
#if HAVE_FCNTL_H
  #include <fcntl.h>
#endif
#include <errno.h>
#include <stdlib.h>
#if (defined(HAVE_LIBC_H) && (!defined(HAVE_UNISTD_H)))
  #include <libc.h>
#else
  #if (!(defined(_MSC_VER)))
    #include <unistd.h>
  #endif
#endif
#if HAVE_STRING_H
  #include <string.h>
#endif

#if HAVE_SAM_9407
  #include <sys/sam9407.h>
#endif

#ifdef MUS_MAC_OSX
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
/* these pull in stdbool.h apparently, so they have to precede sndlib.h */
#endif

#include "_sndlib.h"
#include "sndlib-strings.h"

#if (!HAVE_STRERROR)
char *strerror(int errnum)
{
  char *strerrbuf;
  strerrbuf = (char *)CALLOC(LABEL_BUFFER_SIZE, sizeof(char));
  mus_snprintf(strerrbuf, LABEL_BUFFER_SIZE, "io err %d", errnum);
  return(strerrbuf);
}
#endif

#define MUS_STANDARD_ERROR(Error_Type, Error_Message) \
  mus_print("%s\n  [%s[%d] %s]", Error_Message, __FILE__, __LINE__, c__FUNCTION__)

#define MUS_STANDARD_IO_ERROR(Error_Type, IO_Func, IO_Name) \
  mus_print("%s %s: %s\n  [%s[%d] %s]", IO_Func, IO_Name, strerror(errno), __FILE__, __LINE__, c__FUNCTION__)


static char *version_name = NULL;
static bool audio_initialized = false;

static const char *mus_audio_device_names[] = {
  S_mus_audio_default, S_mus_audio_duplex_default, S_mus_audio_adat_in, S_mus_audio_aes_in, S_mus_audio_line_out,
  S_mus_audio_line_in, S_mus_audio_microphone, S_mus_audio_speakers, S_mus_audio_digital_in, S_mus_audio_digital_out,
  S_mus_audio_dac_out, S_mus_audio_adat_out, S_mus_audio_aes_out, S_mus_audio_dac_filter, S_mus_audio_mixer,
  S_mus_audio_line1, S_mus_audio_line2, S_mus_audio_line3, S_mus_audio_aux_input, S_mus_audio_cd,
  S_mus_audio_aux_output, S_mus_audio_spdif_in, S_mus_audio_spdif_out, S_mus_audio_amp, S_mus_audio_srate,
  S_mus_audio_channel, S_mus_audio_format, S_mus_audio_imix, S_mus_audio_igain, S_mus_audio_reclev,
  S_mus_audio_pcm, S_mus_audio_pcm2, S_mus_audio_ogain, S_mus_audio_line, S_mus_audio_synth,
  S_mus_audio_bass, S_mus_audio_treble, S_mus_audio_port, S_mus_audio_samples_per_channel,
  S_mus_audio_direction
};

static const char *mus_audio_device_name(int dev)
{
  if (MUS_AUDIO_DEVICE_OK(dev))
    return(mus_audio_device_names[dev]);
  return("invalid device");
}

#if (!HAVE_OSS) || (HAVE_ALSA)
static const char *mus_audio_format_names[] = {
  "unknown", S_mus_bshort, S_mus_mulaw, S_mus_byte, S_mus_bfloat, S_mus_bint, S_mus_alaw, S_mus_ubyte, S_mus_b24int,
  S_mus_bdouble, S_mus_lshort, S_mus_lint, S_mus_lfloat, S_mus_ldouble, S_mus_ubshort, S_mus_ulshort, S_mus_l24int,
  S_mus_bintn, S_mus_lintn
};

static const char *mus_audio_format_name(int fr)
{
  if (MUS_DATA_FORMAT_OK(fr))
    return(mus_audio_format_names[fr]);
  return("invalid format");
}
#endif

static char *audio_strbuf = NULL; /* previous name "strbuf" collides with Mac OSX global! */
static void pprint(char *str);

int device_channels(int dev);
int device_gains(int dev);

int device_channels(int dev)
{
  float val[4];
#if USE_SND && MUS_DEBUGGING
  XEN res;
  res = XEN_EVAL_C_STRING("(if (defined? 'debugging-device-channels) debugging-device-channels 0)");
  if (XEN_INTEGER_P(res))
    {
      int chans;
      chans = XEN_TO_C_INT(res);
      if (chans > 0) return(chans);
    }
#endif
  mus_audio_mixer_read(dev, MUS_AUDIO_CHANNEL, 0, val);
  return((int)val[0]);
}

int device_gains(int ur_dev)
{
  float val[4];
  int err;
  int dev;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  /* to get hardware gains, read device amp_field and error = none */
  if ((dev == MUS_AUDIO_DAC_FILTER) || (dev == MUS_AUDIO_MIXER)) 
    {
      err = mus_audio_mixer_read(ur_dev, MUS_AUDIO_CHANNEL, 0, val);
#ifdef HAVE_ALSA
      if (err != MUS_NO_ERROR) return(0);
#endif
      return((int)val[0]);
    }
  err = mus_audio_mixer_read(ur_dev, MUS_AUDIO_AMP, 0, val);
  if (err != MUS_NO_ERROR) return(0);
  return(device_channels(ur_dev));
}



/* ------------------------------- SGI ----------------------------------------- */

#ifdef MUS_SGI
#define AUDIO_OK

#include <audio.h>

int mus_audio_systems(void) {return(1);} /* I think more than 1 is possible, but don't have a case to test with */

char *mus_audio_system_name(int system) {return("SGI");}

char *mus_audio_moniker(void) 
{
#ifdef AL_RESOURCE
  return("New SGI audio");
#else
  return("Old SGI audio");
#endif
}

#ifndef AL_RESOURCE
static char *alGetErrorString(int err)
{
  switch (err)
    {
    case AL_BAD_NOT_IMPLEMENTED:   return("not implemented yet");                   break;
    case AL_BAD_PORT:              return("tried to use an invalid port");          break;
    case AL_BAD_CONFIG:            return("tried to use an invalid configuration"); break;
    case AL_BAD_DEVICE:            return("tried to use an invalid device");        break;
    case AL_BAD_DEVICE_ACCESS:     return("unable to access the device");           break;
    case AL_BAD_DIRECTION:         return("invalid direction given for port");      break;
    case AL_BAD_OUT_OF_MEM:        return("operation has run out of memory");       break;
    case AL_BAD_NO_PORTS:          return("not able to allocate a port");           break;
    case AL_BAD_WIDTH:             return("invalid sample width given");            break;
    case AL_BAD_ILLEGAL_STATE:     return("an invalid state has occurred");         break;
    case AL_BAD_QSIZE:             return("attempt to set an invalid queue size");  break;
    case AL_BAD_FILLPOINT:         return("attempt to set an invalid fillpoint");   break;
    case AL_BAD_BUFFER_NULL:       return("null buffer pointer");                   break;
    case AL_BAD_COUNT_NEG:         return("negative count");                        break;
    case AL_BAD_PVBUFFER:          return("param/val buffer doesn't make sense");   break;
    case AL_BAD_BUFFERLENGTH_NEG:  return("negative buffer length");                break;
    case AL_BAD_BUFFERLENGTH_ODD:  return("odd length parameter/value buffer");     break;
    case AL_BAD_CHANNELS:          return("invalid channel specifier");             break;
    case AL_BAD_PARAM:             return("invalid parameter");                     break;
    case AL_BAD_SAMPFMT:           return("attempt to set invalid sample format");  break;
    case AL_BAD_RATE:              return("invalid sample rate token");             break;
    case AL_BAD_TRANSFER_SIZE:     return("invalid size for sample read/write");    break;
    case AL_BAD_FLOATMAX:          return("invalid size for floatmax");             break;
    case AL_BAD_PORTSTYLE:         return("invalid port style");                    break;
    default:                       return("");
    }
}
#endif

static char *sgi_err_buf = NULL;
static mus_print_handler_t *old_handler = NULL;

static void sgi_mus_print(char *msg)
{
  int oserr = oserror();
  if (oserr)
    {
      if (sgi_err_buf == NULL) sgi_err_buf = (char *)CALLOC(PRINT_BUFFER_SIZE, sizeof(char));
      mus_snprintf(sgi_err_buf, PRINT_BUFFER_SIZE, "%s [%s]", msg, alGetErrorString(oserr));
      (*old_handler)(sgi_err_buf);
    }
  else (*old_handler)(msg);
}

static void start_sgi_print(void)
{
  if (old_handler != sgi_mus_print)
    old_handler = mus_print_set_handler(sgi_mus_print);
}

static void end_sgi_print(void)
{
  if (old_handler != sgi_mus_print)
    mus_print_set_handler(old_handler);
  else mus_print_set_handler(NULL);
}


#if AL_RESOURCE
  #define al_free(Line)                  alFreeConfig(config[Line])
  #define al_newconfig()                 alNewConfig()
  #define al_setsampfmt(Line, Format)    alSetSampFmt(Line, Format)
  #define al_setchannels(Line, Chans)    alSetChannels(Line, Chans)
  #define al_setwidth(Line, Width)       alSetWidth(Line, Width)
  #define al_setqueuesize(Line, Size)    alSetQueueSize(Line, Size)
  #define al_openport(Name, Flag, Line)  alOpenPort(Name, Flag, Line)
  #define al_getfilled(Port)             alGetFilled(Port)
  #define al_closeport(Port)             alClosePort(Port)
  #define al_freeconfig(Config)          alFreeConfig(Config)
#else
  #define al_free(Line)                  ALfreeconfig(config[Line]);
  #define al_newconfig()                 ALnewconfig()
  #define al_setsampfmt(Line, Format)    ALsetsampfmt(Line, Format)
  #define al_setchannels(Line, Chans)    ALsetchannels(Line, Chans)
  #define al_setwidth(Line, Width)       ALsetwidth(Line, Width)
  #define al_setqueuesize(Line, Size)    ALsetqueuesize(Line, Size)
  #define al_openport(Name, Flag, Line)  ALopenport(Name, Flag, Line)
  #define al_getfilled(Port)             ALgetfilled(Port)
  #define al_closeport(Port)             ALcloseport(Port)
  #define al_freeconfig(Config)          ALfreeconfig(Config)
#endif


#define RETURN_ERROR_EXIT(Error_Type, Audio_Line, Ur_Error_Message) \
  do { \
      char *Error_Message; Error_Message = Ur_Error_Message; \
      if (Audio_Line != -1) al_free(Audio_Line); \
      if (Error_Message) \
        { \
          MUS_STANDARD_ERROR(Error_Type, Error_Message); FREE(Error_Message); \
        } \
      else MUS_STANDARD_ERROR(Error_Type, mus_error_type_to_string(Error_Type)); \
      end_sgi_print(); \
      return(MUS_ERROR); \
      } while (false)


#ifdef AL_RESOURCE

static int check_queue_size(int size, int chans) 
{
  if (size > chans * 1024)
    return(size);
  else return(chans * 1024);
}

#else

#define STEREO_QUEUE_MIN_SIZE 1024
#define STEREO_QUEUE_MIN_CHOICE 1024
/* docs say 510 or 512, but they die with "File size limit exceeded" %$@#!(& */
#define MONO_QUEUE_MIN_SIZE 1019
#define MONO_QUEUE_MIN_CHOICE 1024
#define STEREO_QUEUE_MAX_SIZE 131069
#define STEREO_QUEUE_MAX_CHOICE 65536
#define MONO_QUEUE_MAX_SIZE 262139
#define MONO_QUEUE_MAX_CHOICE 131072
/* if these limits are not followed, the damned thing dumps core and dies */

static int check_queue_size(int size, int chans)
{
  if ((chans == 1) && (size > MONO_QUEUE_MAX_SIZE)) return(MONO_QUEUE_MAX_CHOICE);
  if ((chans == 1) && (size < MONO_QUEUE_MIN_SIZE)) return(MONO_QUEUE_MIN_CHOICE);
  if ((chans > 1) && (size > STEREO_QUEUE_MAX_SIZE)) return(STEREO_QUEUE_MAX_CHOICE);
  if ((chans > 1) && (size < STEREO_QUEUE_MIN_SIZE)) return(STEREO_QUEUE_MIN_CHOICE);
  return(size);
}

static void check_quad(int device, int channels)
{
  long sr[2];
  /* if quad, make sure we are set up for it, else make sure we aren't (perhaps the latter is unnecessary) */
  /* in 4 channel mode, stereo mic and line-in are 4 inputs, headphones/speakers and stereo line-out are the 4 outputs */
  sr[0] = AL_CHANNEL_MODE;
  ALgetparams(device, sr, 2);
  if ((channels == 4) && (sr[1] != AL_4CHANNEL))
    {
      sr[1] = AL_4CHANNEL;
      ALsetparams(device, sr, 2);
    }
  else
    {
      if ((channels != 4) && (sr[1] != AL_STEREO))
        {
          sr[1] = AL_STEREO;
          ALsetparams(device, sr, 2);
        }
    }
}
#endif

#define IO_LINES 8
static ALconfig *config = NULL;
static ALport *port = NULL;
static int *line_in_use = NULL;
static int *channels = NULL;
static long *device = NULL;
static int *datum_size = NULL;
static int *line_out = NULL;

int mus_audio_initialize(void)
{
  if (!audio_initialized)
    {
      audio_initialized = true;
      config = (ALconfig *)CALLOC(IO_LINES, sizeof(ALconfig));
      port = (ALport *)CALLOC(IO_LINES, sizeof(ALport));
      line_in_use = (int *)CALLOC(IO_LINES, sizeof(int));
      channels = (int *)CALLOC(IO_LINES, sizeof(int));
      device = (long *)CALLOC(IO_LINES, sizeof(long));
      datum_size = (int *)CALLOC(IO_LINES, sizeof(int));
      line_out = (int *)CALLOC(IO_LINES, sizeof(int));
    }
  return(MUS_NO_ERROR);
}

#ifdef AL_RESOURCE
static int to_al_interface_or_device(int dev, int which)
{
  switch (dev)
    {
    case MUS_AUDIO_DEFAULT:
    case MUS_AUDIO_DUPLEX_DEFAULT: return(AL_DEFAULT_OUTPUT);                                 break;
    case MUS_AUDIO_DAC_OUT:
    case MUS_AUDIO_SPEAKERS:       return(alGetResourceByName(AL_SYSTEM, "Analog Out", which)); break;
    case MUS_AUDIO_MICROPHONE:     return(alGetResourceByName(AL_SYSTEM, "Microphone", which)); break;
    case MUS_AUDIO_ADAT_IN:        return(alGetResourceByName(AL_SYSTEM, "ADAT In", which));    break;
    case MUS_AUDIO_AES_IN:         return(alGetResourceByName(AL_SYSTEM, "AES In", which));     break;
    case MUS_AUDIO_ADAT_OUT:       return(alGetResourceByName(AL_SYSTEM, "ADAT Out", which));   break;
    case MUS_AUDIO_DIGITAL_OUT:
    case MUS_AUDIO_AES_OUT:        return(alGetResourceByName(AL_SYSTEM, "AES Out", which));    break;
    case MUS_AUDIO_LINE_IN:        return(alGetResourceByName(AL_SYSTEM, "Line In", which));    break;
    case MUS_AUDIO_LINE_OUT:       return(alGetResourceByName(AL_SYSTEM, "Line Out2", which));  break; /* ?? */
      /* case MUS_AUDIO_DIGITAL_IN: return(alGetResourceByName(AL_SYSTEM, "DAC2 In", which));   break; */ /* this is analog in ?? */
    }
  return(MUS_ERROR);
}

static int to_al_device(int dev) 
{
  return(to_al_interface_or_device(dev, AL_DEVICE_TYPE));
}

static int to_al_interface(int dev) 
{
  return(to_al_interface_or_device(dev, AL_INTERFACE_TYPE));
}
#endif

#include <stdio.h>

/* just a placeholder for now */
int find_audio_output(int chans)
{
#ifdef AL_RESOURCE
  ALvalue x[32];
  ALpv y;
  int n, i;
  y.param = AL_INTERFACE;
  y.value.i = AL_DIGITAL_IF_TYPE;
  n = alQueryValues(AL_SYSTEM, AL_DEFAULT_OUTPUT, x, 32, &y, 1);
  for (i = 0; i < n; i++) 
    {
      y.param = AL_CHANNELS;
      alGetParams(x[i].i, &y, 1);
      if (chans <= y.value.i) return(x[i].i);
    }
#endif
  return(MUS_ERROR);
}

static int to_sgi_format(int frm)
{
  switch (frm)
    {
    case MUS_BYTE: 
    case MUS_BSHORT: 
    case MUS_B24INT: return(AL_SAMPFMT_TWOSCOMP); break;
    case MUS_BFLOAT: return(AL_SAMPFMT_FLOAT); break;
    case MUS_BDOUBLE: return(AL_SAMPFMT_DOUBLE); break;
    }
  return(MUS_ERROR);
}

int mus_audio_open_output(int ur_dev, int srate, int chans, int format, int requested_size)
{
#ifdef AL_RESOURCE
  ALpv z[2];
#endif
  long sr[2];
  int i, line, size, width, sgi_format, dev;
  start_sgi_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  line = -1;
  for (i = 0; i < IO_LINES; i++) 
    if (line_in_use[i] == 0) 
      {
	line = i; 
	break;
      }
  if (line == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_NO_LINES_AVAILABLE, line,
		      "no free audio lines?");
  channels[line] = chans;
  line_out[line] = 1;

  if (requested_size == 0) 
    size = 1024 * chans;
  else size = check_queue_size(requested_size, chans);
  /* if (chans > 2) size = 65536; */                            /* for temp adat code */

  datum_size[line] = mus_bytes_per_sample(format);
  if (datum_size[line] == 3) 
    width = AL_SAMPLE_24;
  else 
    {
      if (datum_size[line] == 1) 
        width = AL_SAMPLE_8;
      else width = AL_SAMPLE_16;
    }
  sgi_format = to_sgi_format(format);
  if (sgi_format == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d (%s) not supported by SGI",
				 format, 
				 mus_audio_format_name(format)));
#ifdef AL_RESOURCE
  if (dev == MUS_AUDIO_DEFAULT)
    device[line] = AL_DEFAULT_OUTPUT;
  else device[line] = to_al_device(dev);
  if (!(device[line])) 
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, -1,
		      mus_format("device %d (%s) not available",
				 dev,
				 mus_audio_device_name(dev)));
#if 0
  if (device_channels(dev) < chans)                     /* look for some device that can play this file */
    device[line] = find_audio_output(chans);
  if (device[line] == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, -1,
		      mus_format("can't find %d channel device",
				 chans));
#endif
  if ((chans == 4) && (dev == MUS_AUDIO_DAC_OUT))
    {                                                   /* kludge around a bug in the new audio library */
      sr[0] = AL_CHANNEL_MODE;
      sr[1] = AL_4CHANNEL;
      ALsetparams(AL_DEFAULT_DEVICE, sr, 2);
    }
  z[0].param = AL_RATE;
  z[0].value.ll = alDoubleToFixed((double)srate);
  z[1].param = AL_MASTER_CLOCK;
  /* z[1].value.i = AL_CRYSTAL_MCLK_TYPE; */
  z[1].value.i = AL_MCLK_TYPE;                          /* was AL_CRYSTAL_MCLK_TYPE -- digital I/O perhaps needs AL_VARIABLE_MCLK_TYPE */
  if (alSetParams(device[line], z, 2) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, -1,
		      mus_format("can't set srate of %s to %d",
				 mus_audio_device_name(dev),
				 srate));
#else
  device[line] = AL_DEFAULT_DEVICE;
  check_quad(device[line], chans);
  sr[0] = AL_OUTPUT_RATE;
  sr[1] = srate;
  if (ALsetparams(device[line], sr, 2) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, -1,
		      mus_format("can't set srate of %s to %d",
				 mus_audio_device_name(dev),
				 srate));
#endif

  config[line] = al_newconfig();
  if (!(config[line])) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, -1,
		      "can't allocate audio configuration?");
  if ((al_setsampfmt(config[line], sgi_format) == -1) ||
      (al_setwidth(config[line], width) == -1))           /* this is a no-op in the float and double cases */
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, line,
		      mus_format("audio format %d (%s, SGI: %d) not available on device %d (%s)",
				 format, mus_audio_format_name(format), sgi_format,
				 dev, 
				 mus_audio_device_name(dev)));
  if (al_setchannels(config[line], chans) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, line,
		      mus_format("can't get %d channels on device %d (%s)",
				 chans, dev, mus_audio_device_name(dev)));

  /* set queue size probably needs a check first for legal queue sizes given the current desired device */
  /*   in new AL, I'm assuming above (check_queue_size) that it needs at least 1024 per chan */
  if (al_setqueuesize(config[line], size) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_SIZE_NOT_AVAILABLE, line,
		      mus_format("can't get queue size %d on device %d (%s) (chans: %d, requested_size: %d)",
				 size, dev, 
				 mus_audio_device_name(dev), 
				 chans, requested_size));

#ifdef AL_RESOURCE
  if (alSetDevice(config[line], device[line]) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, line,
		      mus_format("can't get device %d (%s)",
				 dev, 
				 mus_audio_device_name(dev)));
#endif

  port[line] = al_openport("dac", "w", config[line]);
  if (!(port[line])) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, line,
		      mus_format("can't open output port on device %d (%s)",
				 dev, 
				 mus_audio_device_name(dev)));
  line_in_use[line] = 1;
  end_sgi_print();
  return(line);
}

int mus_audio_write(int line, char *buf, int bytes)
{
  start_sgi_print();
#ifdef AL_RESOURCE
  if (alWriteFrames(port[line], (short *)buf, bytes / (channels[line] * datum_size[line])))
#else
  if (ALwritesamps(port[line], (short *)buf, bytes / datum_size[line]))
#endif
    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
		      "write error");
  end_sgi_print();
  return(MUS_NO_ERROR);
}

int mus_audio_close(int line)
{
  int err;
  start_sgi_print();
  if (line_in_use[line])
    {
      if (line_out[line])
	while (al_getfilled(port[line]) > 0) 
	  sginap(1);
      err = ((al_closeport(port[line])) || 
	     (al_freeconfig(config[line])));
      line_in_use[line] = 0;
      if (err) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_CLOSE, -1,
			  mus_format("can't close audio port %p (line %d)",
				     port[line], line));
    }
  end_sgi_print();
  return(MUS_NO_ERROR);
}

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int requested_size)
{
  int dev;
#ifdef AL_RESOURCE
  ALpv pv;
  ALpv x[2];
#else
  long sr[2];
  int resind;
#endif
  int i, line, sgi_format;
  start_sgi_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  line = -1;
  for (i = 0; i < IO_LINES; i++) 
    if (line_in_use[i] == 0) 
      {
	line = i; 
	break;
      }
  if (line == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_NO_LINES_AVAILABLE, -1,
		      "no free audio lines?");
  channels[line] = chans;
  line_out[line] = 0;
  datum_size[line] = mus_bytes_per_sample(format);
#ifdef AL_RESOURCE
  if (dev == MUS_AUDIO_DEFAULT)
    device[line] = AL_DEFAULT_INPUT;
  else 
    {
      int itf;
      device[line] = to_al_device(dev);
      itf = to_al_interface(dev);
      if (itf)
	{ 
	  pv.param = AL_INTERFACE;
	  pv.value.i = itf;
	  if (alSetParams(device[line], &pv, 1) == -1)
	    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, -1,
			      mus_format("can't set up device %d (%s)",
					 dev, 
					 mus_audio_device_name(dev)));
	}
    }
  if (!(device[line])) 
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, -1,
		      mus_format("can't get input device %d (%s)",
				 dev, mus_audio_device_name(dev)));
  x[0].param = AL_RATE;
  x[0].value.ll = alDoubleToFixed((double)srate);
  x[1].param = AL_MASTER_CLOCK;
  x[1].value.i = AL_MCLK_TYPE; /* AL_CRYSTAL_MCLK_TYPE; */
  if (alSetParams(device[line], x, 2) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, -1,
		      mus_format("can't set srate of %s to %d",
				 mus_audio_device_name(dev),
				 srate));
#else
  switch (dev)
    {
    case MUS_AUDIO_DEFAULT: 
    case MUS_AUDIO_DUPLEX_DEFAULT: 
    case MUS_AUDIO_MICROPHONE: resind = AL_INPUT_MIC; break;
    case MUS_AUDIO_LINE_IN: resind = AL_INPUT_LINE; break;
    case MUS_AUDIO_DIGITAL_IN: resind = AL_INPUT_DIGITAL; break;
    default: 
      RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, -1,
			mus_format("audio input device %d (%s) not available",
				   dev, 
				   mus_audio_device_name(dev)));
      break;
    }
  device[line] = AL_DEFAULT_DEVICE;
  sr[0] = AL_INPUT_SOURCE;
  sr[1] = resind;
  if (ALsetparams(device[line], sr, 2) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, -1,
		      mus_format("can't set up input device %d (%s)",
				 dev, 
				 mus_audio_device_name(dev)));
  check_quad(device[line], chans);
  sr[0] = AL_INPUT_RATE;
  sr[1] = srate;
  if (ALsetparams(device[line], sr, 2) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, -1,
		      mus_format("can't set srate of %s to %d",
				 mus_audio_device_name(dev),
				 srate));
#endif

  config[line] = al_newconfig();
  if (!(config[line])) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, -1,
		      "can't allocate audio configuration?");
  sgi_format = to_sgi_format(format);
  if (sgi_format == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d (%s) not supported by SGI",
				 format, 
				 mus_audio_format_name(format)));
  if ((al_setsampfmt(config[line], sgi_format) == -1) ||
      (al_setwidth(config[line], (datum_size[line] == 2) ? AL_SAMPLE_16 : AL_SAMPLE_8) == -1))
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, line,
		      mus_format("audio format %d (%s, SGI: %d) not available on device %d (%s)",
				 format, 
				 mus_audio_format_name(format), sgi_format,
				 dev,
				 mus_audio_device_name(dev)));
  if (al_setchannels(config[line], chans) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, line,
		      mus_format("can't get %d channels on device %d (%s)",
				 chans, dev, 
				 mus_audio_device_name(dev)));

#ifdef AL_RESOURCE
  if (alSetDevice(config[line], device[line]) == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, line,
		      mus_format("can't get device %d (%s)",
				 dev, 
				 mus_audio_device_name(dev)));
#endif

  port[line] = al_openport("adc", "r", config[line]);
  if (!(port[line])) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, line,
		      mus_format("can't open input port on device %d (%s)",
				 dev, 
				 mus_audio_device_name(dev)));
  line_in_use[line] = 1;
  end_sgi_print();
  return(line);
}

int mus_audio_read(int line, char *buf, int bytes)
{
  start_sgi_print();
#ifdef AL_RESOURCE
  if (alReadFrames(port[line], (short *)buf, bytes / (channels[line] * datum_size[line])))
#else
  if (ALreadsamps(port[line], (short *)buf, bytes / datum_size[line]))
#endif
    RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
		      "read error");
  end_sgi_print();
  return(MUS_NO_ERROR);
}


#ifdef AL_RESOURCE
/* borrowed from /usr/share/src/dmedia/audio/printdevs.c with modifications */

#define MAX_CHANNELS 8

static float dB_to_linear(float val)
{
  if (val == 0.0) return(1.0);
  return(pow(10.0, val / 20.0));
}

static float dB_to_normalized(float val, float lo, float hi)
{
  float linlo;
  if (hi <= lo) return(1.0);
  linlo = dB_to_linear(lo);
  return((dB_to_linear(val) - linlo) / (dB_to_linear(hi) - linlo));
}

static float normalized_to_dB(float val_norm, float lo_dB, float hi_dB)
{
  if (hi_dB <= lo_dB) return(0.0);
  return(lo_dB + (hi_dB - lo_dB) * val_norm);
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val)
{
  ALpv x[4];
  ALparamInfo pinf;
  ALfixed g[MAX_CHANNELS];
  int rv = 0, i, dev;
  start_sgi_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  if (field != MUS_AUDIO_PORT)
    {
      rv = to_al_device(dev);
      if (!rv) 
	RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, -1,
			  mus_format("can't read %s field %d (%s)",
				     mus_audio_device_name(dev),
				     field, 
				     mus_audio_device_name(field)));
    }
  switch (field)
    {
    case MUS_AUDIO_PORT:
      /* in this case, chan == length of incoming val array.  Number of devices is returned as val[0],
       * and the rest of the available area (if needed) is filled with the device ids.
       */
      i = 0;
      if (alGetResourceByName(AL_SYSTEM, "Microphone", AL_DEVICE_TYPE) != 0) {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_MICROPHONE; i++;}
      if (alGetResourceByName(AL_SYSTEM, "Analog Out", AL_DEVICE_TYPE) != 0) {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_DAC_OUT;    i++;}
      if (alGetResourceByName(AL_SYSTEM, "ADAT In", AL_DEVICE_TYPE) != 0)    {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_ADAT_IN;    i++;}
      if (alGetResourceByName(AL_SYSTEM, "AES In", AL_DEVICE_TYPE) != 0)     {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_AES_IN;     i++;}
      if (alGetResourceByName(AL_SYSTEM, "ADAT Out", AL_DEVICE_TYPE) != 0)   {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_ADAT_OUT;   i++;}
      if (alGetResourceByName(AL_SYSTEM, "AES Out", AL_DEVICE_TYPE) != 0)    {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_AES_OUT;    i++;}
      if (alGetResourceByName(AL_SYSTEM, "Line In", AL_DEVICE_TYPE) != 0)    {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_LINE_IN;    i++;}
      /* if (alGetResourceByName(AL_SYSTEM, "DAC2 In", AL_DEVICE_TYPE) != 0) {if ((i + 1) < chan) val[i + 1] = MUS_AUDIO_DIGITAL_IN; i++;} */
      val[0] = i;
      break;
    case MUS_AUDIO_FORMAT:  
      val[0] = 1; 
      if (chan > 1) val[1] = MUS_BSHORT; 
      break;
    case MUS_AUDIO_CHANNEL:
      x[0].param = AL_CHANNELS;
      if (alGetParams(rv, x, 1) == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			  mus_format("can't read channel setting of %s",
				     mus_audio_device_name(dev)));
      val[0] = (float)(x[0].value.i);
      break;
    case MUS_AUDIO_SRATE:
      x[0].param = AL_RATE;
      if (alGetParams(rv, x, 1) == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			  mus_format("can't read srate setting of %s",
				     mus_audio_device_name(dev)));
      val[0] = (float)(x[0].value.i);
      break;
    case MUS_AUDIO_AMP:
      if (alGetParamInfo(rv, AL_GAIN, &pinf) == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			  mus_format("can't read gain settings of %s",
				     mus_audio_device_name(dev)));
      if (pinf.min.ll == pinf.max.ll) 
	RETURN_ERROR_EXIT(MUS_AUDIO_AMP_NOT_AVAILABLE, -1,
			  mus_format("%s's gain apparently can't be set",
				     mus_audio_device_name(dev)));
      /* this ridiculous thing is in dB with completely arbitrary min and max values */
      x[0].param = AL_GAIN;
      x[0].value.ptr = g;
      x[0].sizeIn = MAX_CHANNELS;
      alGetParams(rv, x, 1);
      if (x[0].sizeOut <= chan) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, -1,
			  mus_format("can't read gain settings of %s channel %d",
				     mus_audio_device_name(dev), chan));
      val[0] = dB_to_normalized(alFixedToDouble(g[chan]),
				alFixedToDouble(pinf.min.ll),
				alFixedToDouble(pinf.max.ll));
      break;
    default: 
      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1,
			  mus_format("can't read %s setting of %s",
				     mus_audio_device_name(field),
				     mus_audio_device_name(dev)));
      break;
    }
  end_sgi_print();
  return(MUS_NO_ERROR);
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val)
{
  /* each field coming in assumes 0.0 to 1.0 as the range */
  ALpv x[4];
  ALparamInfo pinf;
  ALfixed g[MAX_CHANNELS];
  int rv, dev;
  start_sgi_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  rv = to_al_device(dev);
  if (!rv) RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, -1,
			  mus_format("can't write %s field %d (%s)",
				     mus_audio_device_name(dev),
				     field, 
				     mus_audio_device_name(field)));
  switch (field)
    {
    case MUS_AUDIO_SRATE:
      x[0].param = AL_RATE;
      x[0].value.i = (int)val[0];
      x[1].param = AL_MASTER_CLOCK;
      x[1].value.i = AL_CRYSTAL_MCLK_TYPE;
      alSetParams(rv, x, 2); 
      break;
    case MUS_AUDIO_AMP:
      /* need to change normalized linear value into dB between (dB) lo and hi */
      if (alGetParamInfo(rv, AL_GAIN, &pinf) == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			  mus_format("can't write gain settings of %s",
				     mus_audio_device_name(dev)));
      /* I think we need to read all channels here, change the one we care about, then write all channels */
      x[0].param = AL_GAIN;
      x[0].value.ptr = g;
      x[0].sizeIn = MAX_CHANNELS;
      alGetParams(rv, x, 1);
      if (x[0].sizeOut <= chan) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, -1,
			  mus_format("can't write gain settings of %s channel %d",
				     mus_audio_device_name(dev), 
				     chan));
      g[chan] = alDoubleToFixed(normalized_to_dB(val[0], 
						 alFixedToDouble(pinf.min.ll),
						 alFixedToDouble(pinf.max.ll)));
      if (alSetParams(rv, x, 1) == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			  mus_format("can't write gain settings of %s",
				     mus_audio_device_name(dev)));
      break;
    default: 
      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1,
			  mus_format("can't write %s setting of %s",
				     mus_audio_device_name(field),
				     mus_audio_device_name(dev)));
      break;
    }
  end_sgi_print();
  return(MUS_NO_ERROR);
}

#define STRING_SIZE 32
static void dump_resources(ALvalue *x, int rv)
{
  ALpv y[4];
  ALparamInfo pinf;
  ALfixed g[MAX_CHANNELS];
  char dn[STRING_SIZE];
  char dl[STRING_SIZE];
  int i, k;
  ALvalue z[16];
  int nres;
  for (i = 0; i < rv; i++)
    {
      y[0].param = AL_LABEL;
      y[0].value.ptr = dl;
      y[0].sizeIn = STRING_SIZE;
      y[1].param = AL_NAME;
      y[1].value.ptr = dn;
      y[1].sizeIn = STRING_SIZE;
      y[2].param = AL_CHANNELS;
      y[3].param = AL_RATE;
      alGetParams(x[i].i, y, 5);
      if (alIsSubtype(AL_DEVICE_TYPE, x[i].i)) 
        {
          alGetParamInfo(x[i].i, AL_GAIN, &pinf);
          mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "\nDevice: %s (%s), srate: %d, chans: %d",
                  dn, dl,
                  y[3].value.i, 
		  y[2].value.i); 
          pprint(audio_strbuf);
          if (pinf.min.ll != pinf.max.ll)
            {
	      ALpv yy;
              yy.param = AL_GAIN;
              yy.value.ptr = g;
              yy.sizeIn = MAX_CHANNELS;
              alGetParams(x[i].i, &yy, 1);
              pprint(" amps:[");
              for (k = 0; k < yy.sizeOut; k++)
                {
                  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%.2f",
			  dB_to_normalized(alFixedToDouble(g[k]),
					   alFixedToDouble(pinf.min.ll),
					   alFixedToDouble(pinf.max.ll)));
                  pprint(audio_strbuf);
                  if (k < (yy.sizeOut - 1)) pprint(" ");
                }
              pprint("]");
            }
          pprint("\n");
          if ((nres= alQueryValues(x[i].i, AL_INTERFACE, z, 16, 0, 0)) >= 0) 
            dump_resources(z, nres);
          else mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "query failed: %s\n", alGetErrorString(oserror())); 
          pprint(audio_strbuf);
        }
      else 
        {
          mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "      %s (%s), chans: %d\n", dn, dl, y[2].value.i); 
          pprint(audio_strbuf);
        }
    }
}

static void describe_audio_state_1(void)
{
  int rv;
  ALvalue x[16];
  pprint("Devices and Interfaces on this system:\n"); 
  rv= alQueryValues(AL_SYSTEM, AL_DEVICES, x, 16, 0, 0);
  if (rv > 0) 
    dump_resources(x, rv);
}


#else

/* old audio library */

#define MAX_VOLUME 255

static int decode_field(int dev, int field, int chan)
{
  switch (dev)
    {
    case MUS_AUDIO_DEFAULT: 
    case MUS_AUDIO_DAC_OUT:
    case MUS_AUDIO_DUPLEX_DEFAULT:
    case MUS_AUDIO_SPEAKERS:
      switch (field)
        {
        case MUS_AUDIO_AMP:
          return((chan == 0) ? AL_LEFT_SPEAKER_GAIN : AL_RIGHT_SPEAKER_GAIN);
          break;
        case MUS_AUDIO_SRATE:
          return(AL_OUTPUT_RATE);
          break;
        }
      break;
    case MUS_AUDIO_LINE_OUT:
      switch (field)
        {
        case MUS_AUDIO_SRATE:
          return(AL_OUTPUT_RATE); /* ? */
          break;
        }
      break;
    case MUS_AUDIO_DIGITAL_OUT:
      if (field == MUS_AUDIO_SRATE)
        return(AL_OUTPUT_RATE);
      break;
    case MUS_AUDIO_DIGITAL_IN:
      if (field == MUS_AUDIO_SRATE)
        return(AL_INPUT_RATE);
      break;
    case MUS_AUDIO_LINE_IN:
      if (field == MUS_AUDIO_AMP)
        return((chan == 0) ? AL_LEFT_INPUT_ATTEN : AL_RIGHT_INPUT_ATTEN);
      else
	if (field == MUS_AUDIO_SRATE)
	  return(AL_INPUT_RATE);
      break;
    case MUS_AUDIO_MICROPHONE:
      if (field == MUS_AUDIO_AMP)
        return((chan == 0) ? AL_LEFT2_INPUT_ATTEN : AL_RIGHT2_INPUT_ATTEN);
      else
	if (field == MUS_AUDIO_SRATE)
	  return(AL_INPUT_RATE);
      break;
    }
  return(MUS_ERROR);
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val)
{
  long pb[4];
  long fld;
  int dev, err = MUS_NO_ERROR;
  start_sgi_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  switch (field)
    {
    case MUS_AUDIO_CHANNEL:
      val[0] = 4;
      break;
    case MUS_AUDIO_FORMAT:  
      val[0] = 1; 
      if (chan > 1) val[1] = MUS_BSHORT; 
      break;
    case MUS_AUDIO_PORT:
                            /* how to tell which machine we're on? */
      val[0] = 4; 
      if (chan > 1) val[1] = MUS_AUDIO_LINE_IN; 
      if (chan > 2) val[2] = MUS_AUDIO_MICROPHONE; 
      if (chan > 3) val[3] = MUS_AUDIO_DIGITAL_IN; 
      if (chan > 4) val[4] = MUS_AUDIO_DAC_OUT;
      /* does this order work for digital input as well? (i.e. does it replace the microphone)? */
      break;
    case MUS_AUDIO_AMP:
      fld = decode_field(dev, field, chan);
      if (fld != MUS_ERROR)
        {
          pb[0] = fld;
          if (ALgetparams(AL_DEFAULT_DEVICE, pb, 2)) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			      mus_format("can't read gain settings of %s",
					 mus_audio_device_name(dev)));
          if ((fld == AL_LEFT_SPEAKER_GAIN) || 
	      (fld == AL_RIGHT_SPEAKER_GAIN))
            val[0] = ((float)pb[1]) / ((float)MAX_VOLUME);
          else val[0] = 1.0 - ((float)pb[1]) / ((float)MAX_VOLUME);
        }
      else err = MUS_ERROR;
      break;
    case MUS_AUDIO_SRATE:
      fld = decode_field(dev, field, chan);
      if (fld != MUS_ERROR)
        {
          pb[0] = fld;
          if (ALgetparams(AL_DEFAULT_DEVICE, pb, 2)) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			      mus_format("can't read srate setting of %s",
					 mus_audio_device_name(dev)));
          val[0] = pb[1];
        }
      else err = MUS_ERROR;
      break;
    default: 
      err = MUS_ERROR;
      break;
    }
  if (err == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1,
		      mus_format("can't read %s setting of %s",
				 mus_audio_device_name(field),
				 mus_audio_device_name(dev)));
  end_sgi_print();
  return(MUS_NO_ERROR);
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val)
{
  long pb[4];
  long fld;
  int dev, err = MUS_NO_ERROR;
  start_sgi_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  switch (field)
    {
    case MUS_AUDIO_PORT:
      if (dev == MUS_AUDIO_DEFAULT)
        {
	  pb[0] = AL_CHANNEL_MODE;
	  pb[1] = ((chan == MUS_AUDIO_DIGITAL_IN) ? AL_STEREO : AL_4CHANNEL);
          pb[2] = AL_INPUT_SOURCE;
          pb[3] = ((chan == MUS_AUDIO_DIGITAL_IN) ? AL_INPUT_DIGITAL : AL_INPUT_MIC);
          if (ALsetparams(AL_DEFAULT_DEVICE, pb, 4)) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			      mus_format("can't set mode and source of %s",
					 mus_audio_device_name(dev)));
        }
      else err = MUS_ERROR;
      break;
    case MUS_AUDIO_CHANNEL:
      if (dev == MUS_AUDIO_MICROPHONE)
        {
          pb[0] =  AL_MIC_MODE;
          pb[1] = ((chan == 2) ? AL_STEREO : AL_MONO);
          if (ALsetparams(AL_DEFAULT_DEVICE, pb, 2)) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			      mus_format("can't set microphone to be %s",
					 (chan == 2) ? "stereo" : "mono"));
        }
      else
        {
          if (dev == MUS_AUDIO_DEFAULT)
            {
              pb[0] = AL_CHANNEL_MODE;
              pb[1] = ((chan == 4) ? AL_4CHANNEL : AL_STEREO);
              if (ALsetparams(AL_DEFAULT_DEVICE, pb, 2)) 
		RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
				  mus_format("can't set default device to be %s",
					     (chan == 4) ? "quad" : "stereo"));
            }
	  else err = MUS_ERROR;
        }
      break;
    case MUS_AUDIO_AMP:
      fld = decode_field(dev, field, chan);
      if (fld != -1)
        {
          pb[0] = fld;
          if ((fld == AL_LEFT_SPEAKER_GAIN) || 
	      (fld == AL_RIGHT_SPEAKER_GAIN))
            pb[1] = val[0] * MAX_VOLUME;
          else pb[1] =  (1.0 - val[0]) * MAX_VOLUME;
          if (ALsetparams(AL_DEFAULT_DEVICE, pb, 2)) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			      mus_format("can't set gain of %s",
					 mus_audio_device_name(dev)));
        }
      else err = MUS_ERROR;
      break;
    case MUS_AUDIO_SRATE:
      fld = decode_field(dev, field, chan);
      if (fld != -1)
        {
          pb[0] = fld;
          pb[1] = val[0];
          if (ALsetparams(AL_DEFAULT_DEVICE, pb, 2)) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1, NULL);
	  if (fld == AL_INPUT_RATE)
	    {
	      pb[0] = AL_OUTPUT_RATE;
	      pb[1] = val[0];
	      if (ALsetparams(AL_DEFAULT_DEVICE, pb, 2)) 
		RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
				  mus_format("can't set srate of %s",
					     mus_audio_device_name(dev)));
            }
        }
      else err = MUS_ERROR;
      break;
    default: 
      err = MUS_ERROR;
      break;
    }
    if (err == MUS_ERROR)
      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, -1,
			mus_format("can't write %s setting of %s",
				   mus_audio_device_name(field),
				   mus_audio_device_name(dev)));
  end_sgi_print();
  return(MUS_NO_ERROR);
}

static void describe_audio_state_1(void) 
{
  float amps[1];
  int err;
  err = mus_audio_mixer_read(MUS_AUDIO_SPEAKERS, MUS_AUDIO_SRATE, 0, amps);
  if (err == MUS_NO_ERROR) 
    {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "srate: %.2f\n", amps[0]); pprint(audio_strbuf);} 
  else {fprintf(stdout, "err: %d!\n", err); fflush(stdout);}
  err = mus_audio_mixer_read(MUS_AUDIO_SPEAKERS, MUS_AUDIO_AMP, 0, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "speakers: %.2f", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_SPEAKERS, MUS_AUDIO_AMP, 1, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.2f\n", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_LINE_IN, MUS_AUDIO_AMP, 0, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "line in: %.2f", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_LINE_IN, MUS_AUDIO_AMP, 1, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.2f\n", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_MICROPHONE, MUS_AUDIO_AMP, 0, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "microphone: %.2f", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_MICROPHONE, MUS_AUDIO_AMP, 1, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.2f\n", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_LINE_OUT, MUS_AUDIO_AMP, 0, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "line out: %.2f", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_LINE_OUT, MUS_AUDIO_AMP, 1, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.2f\n", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_DIGITAL_OUT, MUS_AUDIO_AMP, 0, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "digital out: %.2f", amps[0]); pprint(audio_strbuf);}
  err = mus_audio_mixer_read(MUS_AUDIO_DIGITAL_OUT, MUS_AUDIO_AMP, 1, amps);
  if (err == MUS_NO_ERROR) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.2f\n", amps[0]); pprint(audio_strbuf);}
}

#endif
/* new or old AL */

#endif
/* SGI */



/* ------------------------------- OSS ----------------------------------------- */

#if (HAVE_OSS || HAVE_ALSA || HAVE_JACK)
/* actually it's not impossible that someday we'll have ALSA but not OSS... */
#define AUDIO_OK

#include <sys/ioctl.h>

/* the system version of the soundcard header file may have no relation to the current OSS actually loaded */
/* sys/soundcard.h is usually just a pointer to linux/soundcard.h */

#if (MUS_HAVE_USR_LIB_OSS)
  #include "/usr/lib/oss/include/sys/soundcard.h"
#else
  #if (MUS_HAVE_USR_LOCAL_LIB_OSS)
    #include "/usr/local/lib/oss/include/sys/soundcard.h"
  #else
    #if (MUS_HAVE_OPT_OSS)
      #include "/opt/oss/include/sys/soundcard.h"
    #else
      #if (MUS_HAVE_VAR_LIB_OSS)
        #include "/var/lib/oss/include/sys/soundcard.h"
      #else
        #if defined(HAVE_SYS_SOUNDCARD_H) || defined(MUS_LINUX)
          #include <sys/soundcard.h>
        #else
          #if defined(HAVE_MACHINE_SOUNDCARD_H)
            #include <machine/soundcard.h>
          #else
            #include <soundcard.h>
          #endif
        #endif
      #endif
    #endif
  #endif
#endif

#if ((SOUND_VERSION > 360) && (defined(OSS_SYSINFO)))
  #define NEW_OSS 1
#endif

#define DAC_NAME "/dev/dsp"
#define MIXER_NAME "/dev/mixer"
#define SYNTH_NAME "/dev/music"
/* some programs use /dev/audio */

/* there can be more than one sound card installed, and a card can be handled through
 * more than one /dev/dsp device, so we can't use a global dac device here.
 * The caller has to keep track of the various cards (via AUDIO_SYSTEM) --
 * I toyed with embedding all that in mus_audio_open_output and mus_audio_write, but
 * decided it's better to keep them explicit -- the caller may want entirely
 * different (non-synchronous) streams going to the various cards.  This same
 * code (AUDIO_SYSTEM(n)->devn) should work in Windoze (see below), and
 * might work on the Mac and SGI -- something for a rainy day...
 */

#define RETURN_ERROR_EXIT(Message_Type, Audio_Line, Ur_Message) \
  do { \
       char *Message; Message = Ur_Message; \
       if (Audio_Line != -1) \
          linux_audio_close(Audio_Line); \
       if ((Message) && (strlen(Message) > 0)) \
         { \
           mus_print("%s\n  [%s[%d] %s]", \
                     Message, \
                     __FILE__, __LINE__, c__FUNCTION__); \
           FREE(Message); \
         } \
       else mus_print("%s\n  [%s[%d] %s]", \
                      mus_error_type_to_string(Message_Type), \
                      __FILE__, __LINE__, c__FUNCTION__); \
       return(MUS_ERROR); \
     } while (false)

static int FRAGMENTS = 4;
static int FRAGMENT_SIZE = 12;
static bool fragments_locked = false;

/* defaults here are FRAGMENTS 16 and FRAGMENT_SIZE 12; these values however
 * cause about a .5 second delay, which is not acceptable in "real-time" situations.
 *
 * this changed 22-May-01: these are causing more trouble than they're worth
 */

static void oss_mus_oss_set_buffers(int num, int size) {FRAGMENTS = num; FRAGMENT_SIZE = size; fragments_locked = true;}

#define MAX_SOUNDCARDS 8
#define MAX_DSPS 8
#define MAX_MIXERS 8
/* there can be (apparently) any number of mixers and dsps per soundcard, but 8 is enough! */

static int *audio_fd = NULL; 
static int *audio_open_ctr = NULL; 
static int *audio_dsp = NULL; 
static int *audio_mixer = NULL; 
static int *audio_mode = NULL; 
typedef enum {NORMAL_CARD, SONORUS_STUDIO, RME_HAMMERFALL, SAM9407_DSP, DELTA_66} audio_card_t;
/* the Sonorus Studi/o card is a special case in all regards */
static audio_card_t *audio_type = NULL; 

static int sound_cards = 0;
static int new_oss_running = 0;
static char *dev_name = NULL;

static int oss_mus_audio_systems(void) 
{
  return(sound_cards);
}

static char *mixer_name(int sys)
{
#if HAVE_SAM_9407
  if((sys < sound_cards) && (audio_type[sys] == SAM9407_DSP))
    {
      mus_snprintf(dev_name, LABEL_BUFFER_SIZE, "/dev/sam%d_mixer", audio_mixer[sys]);
      return(dev_name);
    }
#endif 
  if (sys < sound_cards)
    {
      if (audio_mixer[sys] == -2)
	return(MIXER_NAME); 
      /* if we have /dev/dsp (not /dev/dsp0), I assume the corresponding mixer is /dev/mixer (not /dev/mixer0) */
      /* but in sam9407 driver, there is no /dev/mixer, and everything goes through /dev/dsp */
      else
	{
	  if (audio_mixer[sys] == -3)
	    return(DAC_NAME); 
	  else 
	    {
	      mus_snprintf(dev_name, LABEL_BUFFER_SIZE, "%s%d", MIXER_NAME, audio_mixer[sys]);
	      return(dev_name);
	    }
	}
    }
  return(DAC_NAME);
}

static char *oss_mus_audio_system_name(int system) 
{
#if HAVE_SAM_9407
  if((system < sound_cards) && (audio_type[system] == SAM9407_DSP))
    {
      int fd;
      fd = open(mixer_name(system), O_RDONLY, 0);
      if(fd != -1) 
	{
	  static SamDriverInfo driverInfo;
	  if(ioctl(fd, SAM_IOC_DRIVER_INFO, &driverInfo) >= 0) 
	    {
	      close(fd);
	      return(driverInfo.hardware);
	    }
	  close(fd);
	}
      return("sam9407");
    }
#endif
#ifdef NEW_OSS
  static mixer_info mixinfo;
  int status, ignored, fd;
  fd = open(mixer_name(system), O_RDONLY, 0);
  if (fd != -1)
    {
      status = ioctl(fd, OSS_GETVERSION, &ignored);
      if (status == 0)
	{
	  status = ioctl(fd, SOUND_MIXER_INFO, &mixinfo);
	  if (status == 0) 
	    {
	      close(fd);
	      return(mixinfo.name);
	    }
	}
      close(fd);
    }
#endif
  return("OSS");
}

#if HAVE_SAM_9407
static char *oss_mus_audio_moniker(void) {return("Sam 9407");}
#else
static char *oss_mus_audio_moniker(void)
{
  char version[LABEL_BUFFER_SIZE];
  if (version_name == NULL) version_name = (char *)CALLOC(LABEL_BUFFER_SIZE, sizeof(char));
  if (SOUND_VERSION < 361)
    {
      mus_snprintf(version, LABEL_BUFFER_SIZE, "%d", SOUND_VERSION);
      mus_snprintf(version_name, LABEL_BUFFER_SIZE, "OSS %c.%c.%c", version[0], version[1], version[2]);
    }
  else
    mus_snprintf(version_name, LABEL_BUFFER_SIZE, "OSS %x.%x.%x", 
		 (SOUND_VERSION >> 16) & 0xff, 
		 (SOUND_VERSION >> 8) & 0xff, 
		 SOUND_VERSION & 0xff);
  return(version_name);
}
#endif

static char *dac_name(int sys, int offset)
{
#if HAVE_SAM_9407
  if ((sys < sound_cards) && (audio_type[sys] == SAM9407_DSP))
    {
      mus_snprintf(dev_name, LABEL_BUFFER_SIZE, "/dev/sam%d_dsp", audio_dsp[sys]);
      return(dev_name);
    }
#endif
  if ((sys < sound_cards) && (audio_mixer[sys] >= -1))
    {
      mus_snprintf(dev_name, LABEL_BUFFER_SIZE, "%s%d", DAC_NAME, audio_dsp[sys] + offset);
      return(dev_name);
    }
  return(DAC_NAME);
}

#define MIXER_SIZE SOUND_MIXER_NRDEVICES
static int **mixer_state = NULL;
static int *init_srate = NULL, *init_chans = NULL, *init_format = NULL;

static int oss_mus_audio_initialize(void) 
{
  /* here we need to set up the map of /dev/dsp and /dev/mixer to a given system */
  /* since this info is not passed to us by OSS, we have to work at it... */
  /* for the time being, I'll ignore auxiliary dsp and mixer ports (each is a special case) */
  int i, fd = -1, md, err = 0;
  char dname[LABEL_BUFFER_SIZE];
  int amp, old_mixer_amp, old_dsp_amp, new_mixer_amp, responsive_field;
  int devmask;
#ifdef NEW_OSS
  int status, ignored;
  oss_sysinfo sysinfo;
  static mixer_info mixinfo;
  int sysinfo_ok = 0;
#endif
  int num_mixers, num_dsps, nmix, ndsp;
  if (!audio_initialized)
    {
      audio_initialized = true;
      audio_fd = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      audio_open_ctr = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      audio_dsp = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      audio_mixer = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      audio_type = (audio_card_t *)CALLOC(MAX_SOUNDCARDS, sizeof(audio_card_t));
      audio_mode = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      dev_name = (char *)CALLOC(LABEL_BUFFER_SIZE, sizeof(char));
      init_srate = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      init_chans = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      init_format = (int *)CALLOC(MAX_SOUNDCARDS, sizeof(int));
      mixer_state = (int **)CALLOC(MAX_SOUNDCARDS, sizeof(int *));
      for (i = 0; i < MAX_SOUNDCARDS; i++) mixer_state[i] = (int *)CALLOC(MIXER_SIZE, sizeof(int));
      for (i = 0; i < MAX_SOUNDCARDS; i++)
	{
	  audio_fd[i] = -1;
	  audio_open_ctr[i] = 0;
	  audio_dsp[i] = -1;
	  audio_mixer[i] = -1;
	  audio_type[i] = NORMAL_CARD;
	}
#if HAVE_SAM_9407
      {
	SamApiInfo apiInfo;
	SamDriverInfo driverInfo;
	for (i = 0; i < MAX_SOUNDCARDS; i++) 
	  {
	    mus_snprintf(dname, LABEL_BUFFER_SIZE, "/dev/sam%d_mixer", i);
	    fd = open(dname, O_WRONLY);
	    if (fd < 0)
	      break;
	    if ((ioctl(fd, SAM_IOC_API_INFO, &apiInfo) < 0) ||
		(apiInfo.apiClass!=SAM_API_CLASS_VANILLA) ||
		(ioctl(fd, SAM_IOC_DRIVER_INFO, &driverInfo) < 0) ||
		(!driverInfo.haveAudio))
	      {
		close(fd);
		continue;
	      }
	    audio_type[sound_cards] = SAM9407_DSP;
	    audio_dsp[sound_cards] = i;
	    audio_mixer[sound_cards] = i;
	    sound_cards++;
	    close(fd);
	  }
	if(sound_cards > 0)
	  return(0);
      }
#endif

      num_mixers = MAX_MIXERS;
      num_dsps = MAX_DSPS;
#ifdef NEW_OSS
      fd = open(DAC_NAME, O_WRONLY | O_NONBLOCK, 0);
      if (fd == -1) fd = open(SYNTH_NAME, O_RDONLY | O_NONBLOCK, 0);
      if (fd == -1) fd = open(MIXER_NAME, O_RDONLY | O_NONBLOCK, 0);
      if (fd != -1)
	{
	  status = ioctl(fd, OSS_GETVERSION, &ignored);
	  new_oss_running = (status == 0);
	  if (new_oss_running)
	    {
	      status = ioctl(fd, OSS_SYSINFO, &sysinfo);
	      sysinfo_ok = (status == 0);
	    }
	  if ((new_oss_running) && (sysinfo_ok))
	    {
	      num_mixers = sysinfo.nummixers;
	      num_dsps = sysinfo.numaudios;
	    }
	  close(fd);
	}
#endif

      /* need to get which /dev/dsp lines match which /dev/mixer lines,
       *   find out how many separate systems (soundcards) are available,
       *   fill the audio_dsp and audio_mixer arrays with the system-related numbers,
       * since we have no way to tell from OSS info which mixers/dsps are the
       *   main ones, we'll do some messing aound to try to deduce this info.
       * for example, SB uses two dsp ports and two mixers per card, whereas
       *  Ensoniq uses 2 dsps and 1 mixer.
       * 
       * the data we are gathering here:
       *   int audio_dsp[MAX_SOUNDCARDS] -> main_dsp_port[MUS_AUDIO_PACK_SYSTEM(n)] (-1 => no such system dsp)
       *   int audio_mixer[MAX_SOUNDCARDS] -> main_mixer_port[MUS_AUDIO_PACK_SYSTEM(n)]
       *   int sound_cards = 0 -> usable systems
       * all auxiliary ports are currently ignored (SB equalizer, etc)
       */
      sound_cards = 0;
      ndsp = 0;
      nmix = 0;
      while ((nmix < num_mixers) && 
	     (ndsp < num_dsps))
	{
	  /* for each mixer, find associated main dsp (assumed to be first in /dev/dsp ordering) */
	  /*   if mixer's dsp overlaps or we run out of dsps first, ignore it (aux mixer) */
	  /* our by-guess-or-by-gosh method here is to try to open the mixer.
	   *   if that fails, quit (if very first, try at least to get the dsp setup)
	   *   find volume field, if none, go on, else read current volume
	   *   open next unchecked dsp, try to set volume, read current, if different we found a match -- set and go on.
	   *     if no change, move to next dsp and try again, if no more dsps, quit (checking for null case as before)
	   */
	  mus_snprintf(dname, LABEL_BUFFER_SIZE, "%s%d", MIXER_NAME, nmix);
	  md = open(dname, O_RDWR, 0);
	  if (md == -1)
	    {
	      if (errno == EBUSY) 
		{
		  mus_print("%s is busy: can't access it [%s[%d] %s]", 
			    dname,
			    __FILE__, __LINE__, c__FUNCTION__); 
		  nmix++;
		  continue;
		}
	      else break;
	    }
	  mus_snprintf(dname, LABEL_BUFFER_SIZE, "%s%d", DAC_NAME, ndsp);
	  fd = open(dname, O_RDWR | O_NONBLOCK, 0);
	  if (fd == -1) fd = open(dname, O_RDONLY | O_NONBLOCK, 0);
 	  if (fd == -1) fd = open(dname, O_WRONLY | O_NONBLOCK, 0); /* some output devices need this */
	  if (fd == -1)
	    {
	      close(md); 
	      if (errno == EBUSY) /* in linux /usr/include/asm/errno.h */
		{
		  fprintf(stderr, "%s is busy: can't access it\n", dname); 
		  ndsp++;
		  continue;
		}
	      else 
		{
		  if ((errno != ENXIO) && (errno != ENODEV))
		    fprintf(stderr, "%s: %s! ", dname, strerror(errno));
		  break;
		}
	    }
#ifdef NEW_OSS				  
	  /* can't change volume yet of Sonorus, so the method above won't work --
	   * try to catch this case via the mixer's name
	   */
	  status = ioctl(md, SOUND_MIXER_INFO, &mixinfo);
	  if ((status == 0) && 
	      (mixinfo.name) && 
	      (*(mixinfo.name)) && 
	      (strlen(mixinfo.name) > 6))
	    {
	      if (strncmp("STUDI/O", mixinfo.name, 7) == 0)
		{
		  /* a special case in every regard */
		  audio_type[sound_cards] = SONORUS_STUDIO;
		  audio_mixer[sound_cards] = nmix; 
		  nmix++;
		  audio_dsp[sound_cards] = ndsp; 
		  if (num_dsps >= 21)
		    {
		      ndsp += 21;
		      audio_mode[sound_cards] = 1;
		    }
		  else
		    {
		      ndsp += 9;
		      audio_mode[sound_cards] = 0;
		    }
		  sound_cards++;
		  close(fd);
		  close(md);
		  continue;
		}
	      else
		{
		  if (strncmp("RME Digi96", mixinfo.name, 10) == 0)
		    {
		      audio_type[sound_cards] = RME_HAMMERFALL;
		      audio_mixer[sound_cards] = nmix; 
		      nmix++;
		      audio_dsp[sound_cards] = ndsp; 
		      sound_cards++;
		      close(fd);
		      close(md);
		      continue;
		    }
		  else
		    {
		      if (strncmp("M Audio Delta", mixinfo.name, 13) == 0)
			{
			  audio_type[sound_cards] = DELTA_66;
			  audio_mixer[sound_cards] = nmix; 
			  nmix++;
			  ndsp += 6; /* just a guess */
			  audio_dsp[sound_cards] = ndsp; 
			  sound_cards++;
			  close(fd);
			  close(md);
			  continue;
			}
		    }
		}
	    }
#endif
	  err = ioctl(md, SOUND_MIXER_READ_DEVMASK, &devmask);
	  responsive_field = SOUND_MIXER_VOLUME;
	  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	    if ((1 << i) & devmask)
	      {
		responsive_field = i;
		break;
	      }
	  if (!err)
	    {
	      err = ioctl(md, MIXER_READ(responsive_field), &old_mixer_amp);
	      if (!err)
		{
		  err = ioctl(fd, MIXER_READ(responsive_field), &old_dsp_amp);
		  if ((!err) && (old_dsp_amp == old_mixer_amp))
		    {
		      if (old_mixer_amp == 0) amp = 50; else amp = 0; /* 0..100 */
		      err = ioctl(fd, MIXER_WRITE(responsive_field), &amp);
		      if (!err)
			{
			  err = ioctl(md, MIXER_READ(responsive_field), &new_mixer_amp);
			  if (!err)
			    {
			      if (new_mixer_amp == amp)
				{
				  /* found one! */
				  audio_dsp[sound_cards] = ndsp; ndsp++;
				  audio_mixer[sound_cards] = nmix; nmix++;
				    audio_type[sound_cards] = NORMAL_CARD;
				  sound_cards++;
				}
			      else ndsp++;
			      err = ioctl(fd, MIXER_WRITE(responsive_field), &old_dsp_amp);
			    }
			  else nmix++;
			}
		      else ndsp++;
		    }
		  else ndsp++;
		}
	      else nmix++;
	    }
	  else nmix++;
	  close(fd);
	  close(md);
	}
      if (sound_cards == 0)
	{
 	  fd = open(DAC_NAME, O_WRONLY | O_NONBLOCK, 0);
	  if (fd != -1)
	    {
	      sound_cards = 1;
	      audio_dsp[0] = 0;
	      audio_type[0] = NORMAL_CARD;
	      audio_mixer[0] = -2; /* hmmm -- need a way to see /dev/dsp as lonely outpost */
	      close(fd);
 	      fd = open(MIXER_NAME, O_RDONLY | O_NONBLOCK, 0);
	      if (fd == -1)
		audio_mixer[0] = -3;
	      else close(fd);
	    }
	}
    }
  return(MUS_NO_ERROR);
}

int mus_audio_reinitialize(void)
{
  /* an experiment */
  audio_initialized = false;
  return(mus_audio_initialize());
}

static int linux_audio_open(const char *pathname, int flags, mode_t mode, int system)
{
  /* sometimes this is simply searching for a device (so failure is not a mus_error) */
  if (audio_fd[system] == -1) 
    {
      audio_fd[system] = open(pathname, flags, mode);
      audio_open_ctr[system] = 0;
    }
  else audio_open_ctr[system]++;
  return(audio_fd[system]);
}

static int linux_audio_open_with_error(const char *pathname, int flags, mode_t mode, int system)
{
  int fd;
  fd = linux_audio_open(pathname, flags, mode, system);
  if (fd == -1)
    MUS_STANDARD_IO_ERROR(MUS_AUDIO_CANT_OPEN,
			  ((mode == O_RDONLY) ? "open read" : 
			   (mode == O_WRONLY) ? "open write" : "open read/write"),
			  pathname);
  return(fd);
}

static int find_system(int line)
{
  int i;
  for (i = 0; i < sound_cards; i++)
    if (line == audio_fd[i])
      return(i);
  return(MUS_ERROR);
}

static int linux_audio_close(int fd)
{
  if (fd != -1)
    {
      int err = 0, sys;
      sys = find_system(fd);
      if (sys != -1)
	{
	  if (audio_open_ctr[sys] > 0) 
	    audio_open_ctr[sys]--;
	  else 
	    {
	      err = close(fd);
	      audio_open_ctr[sys] = 0;
	      audio_fd[sys] = -1;
	    }
	}
      else err = close(fd);
      if (err) RETURN_ERROR_EXIT(MUS_AUDIO_CANT_CLOSE, -1,
				 mus_format("close %d failed: %s",
					    fd, strerror(errno)));
    }
  /* is this an error? */
  return(MUS_NO_ERROR);
}

static int to_oss_format(int snd_format)
{
  switch (snd_format)
    {
    case MUS_BYTE:    return(AFMT_S8);     break;
    case MUS_BSHORT:  return(AFMT_S16_BE); break;
    case MUS_UBYTE:   return(AFMT_U8);     break;
    case MUS_MULAW:   return(AFMT_MU_LAW); break;
    case MUS_ALAW:    return(AFMT_A_LAW);  break;
    case MUS_LSHORT:  return(AFMT_S16_LE); break;
    case MUS_UBSHORT: return(AFMT_U16_BE); break;
    case MUS_ULSHORT: return(AFMT_U16_LE); break;
#ifdef NEW_OSS
    case MUS_LINT:    return(AFMT_S32_LE); break;
    case MUS_BINT:    return(AFMT_S32_BE); break;
#endif
    }
  return(MUS_ERROR);
}

static char sonorus_buf[LABEL_BUFFER_SIZE];
static char *sonorus_name(int sys, int offset)
{
  mus_snprintf(sonorus_buf, LABEL_BUFFER_SIZE, "/dev/dsp%d", offset + audio_dsp[sys]);
  return(sonorus_buf);
}

static bool fragment_set_failed = false;

static int oss_mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size)
{
  /* ur_dev is in general MUS_AUDIO_PACK_SYSTEM(n) | MUS_AUDIO_DEVICE */
  int oss_format, buffer_info, audio_out = -1, sys, dev;
  char *dev_name;
#ifndef NEW_OSS
  int stereo;
#endif
  sys = MUS_AUDIO_SYSTEM(ur_dev);
  dev = MUS_AUDIO_DEVICE(ur_dev);
  oss_format = to_oss_format(format); 
  if (oss_format == MUS_ERROR) 
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d (%s) not available",
				 format, 
				 mus_data_format_name(format)));
  if (audio_type[sys] == SONORUS_STUDIO)
    {
      /* in this case the output devices are parcelled out to the /dev/dsp locs */
      /* dev/dsp0 is always stereo */
      switch (dev)
	{
	case MUS_AUDIO_DEFAULT: 
	  if (chans > 2) 
	    audio_out = open(sonorus_name(sys, 1), O_WRONLY, 0);
	  else audio_out = open(sonorus_name(sys, 0), O_WRONLY, 0);
	  /* probably should write to both outputs */
	  if (audio_out == -1) audio_out = open("/dev/dsp", O_WRONLY, 0);
	  break;
	case MUS_AUDIO_SPEAKERS:
	  audio_out = open(sonorus_name(sys, 0), O_WRONLY, 0);
	  if (audio_out == -1) audio_out = open("/dev/dsp", O_WRONLY, 0);
	  break;
	case MUS_AUDIO_ADAT_OUT: case MUS_AUDIO_SPDIF_OUT:
	  audio_out = open(sonorus_name(sys, 1), O_WRONLY, 0);
	  break;
	case MUS_AUDIO_AES_OUT:
	  audio_out = open(sonorus_name(sys, 9), O_WRONLY, 0);
	  break;
	default:
	  RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, audio_out,
			    mus_format("Sonorus device %d (%s) not available",
				       dev, 
				       mus_audio_device_name(dev)));
	  break;
	}
      if (audio_out == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, audio_out,
			  mus_format("can't open Sonorus output device %d (%s): %s",
				     dev, 
				     mus_audio_device_name(dev), strerror(errno)));
#ifdef NEW_OSS
      if (ioctl(audio_out, SNDCTL_DSP_CHANNELS, &chans) == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_out,
			  mus_format("can't get %d channels for Sonorus device %d (%s)",
				     chans, dev, 
				     mus_audio_device_name(dev)));
#endif
      return(audio_out);
    }

#if HAVE_SAM_9407
  if (audio_type[sys] == SAM9407_DSP)
    {
      char dname[LABEL_BUFFER_SIZE];
      mus_snprintf(dname, LABEL_BUFFER_SIZE, "/dev/sam%d_dsp", audio_dsp[sys]);
      audio_out = open(dname, O_WRONLY);
      if(audio_out == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, audio_out,
			  mus_format("can't open %s: %s",
				     dname, 
				     strerror(errno)));
      if ((ioctl(audio_out, SNDCTL_DSP_SETFMT, &oss_format) == -1) || 
	  (oss_format != to_oss_format(format))) 
	RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_out,
			  mus_format("can't set %s format to %d (%s)",
				     dname, format, 
				     mus_data_format_name(format)));
      if (ioctl(audio_out, SNDCTL_DSP_CHANNELS, &chans) == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_out,
			  mus_format("can't get %d channels on %s",
				     chans, dname));
      if (ioctl(audio_out, SNDCTL_DSP_SPEED, &srate) == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, audio_out,
			  mus_format("can't set srate to %d on %s",
				     srate, dname));
      FRAGMENT_SIZE = 14;
      buffer_info = (FRAGMENTS << 16) | (FRAGMENT_SIZE);
      ioctl(audio_out, SNDCTL_DSP_SETFRAGMENT, &buffer_info);
      return(audio_out);
    }
#endif

  if (dev == MUS_AUDIO_DEFAULT)
    audio_out = linux_audio_open_with_error(dev_name = dac_name(sys, 0), 
					    O_WRONLY, 0, sys);
  else audio_out = linux_audio_open_with_error(dev_name = dac_name(sys, (dev == MUS_AUDIO_AUX_OUTPUT) ? 1 : 0), 
					       O_RDWR, 0, sys);
  if (audio_out == -1) return(MUS_ERROR);

  /* ioctl(audio_out, SNDCTL_DSP_RESET, 0); */ /* causes clicks */
  if ((fragments_locked) && 
      (!(fragment_set_failed)) &&
      ((dev == MUS_AUDIO_DUPLEX_DEFAULT) || 
       (size != 0))) /* only set if user has previously called set_oss_buffers */
    {
      buffer_info = (FRAGMENTS << 16) | (FRAGMENT_SIZE);
      if (ioctl(audio_out, SNDCTL_DSP_SETFRAGMENT, &buffer_info) == -1)
        {
          /* older Linuces (or OSS's?) refuse to handle the fragment reset if O_RDWR used --
           * someone at OSS forgot to update the version number when this was fixed, so
           * I have no way to get around this except to try and retry...
           */
          linux_audio_close(audio_out);
          audio_out = linux_audio_open_with_error(dev_name = dac_name(sys, (dev == MUS_AUDIO_AUX_OUTPUT) ? 1 : 0), 
						  O_WRONLY, 0, sys);
	  if (audio_out == -1) return(MUS_ERROR);
          buffer_info = (FRAGMENTS << 16) | (FRAGMENT_SIZE);
          if (ioctl(audio_out, SNDCTL_DSP_SETFRAGMENT, &buffer_info) == -1) 
	    {
	      char *tmp;
	      fprintf(stderr, tmp = mus_format("can't set %s fragments to: %d x %d",
					       dev_name, FRAGMENTS, FRAGMENT_SIZE)); /* not an error if ALSA OSS-emulation */
	      fragment_set_failed = true;
	      FREE(tmp);
	    }
        }
    }
  if ((ioctl(audio_out, SNDCTL_DSP_SETFMT, &oss_format) == -1) || 
      (oss_format != to_oss_format(format)))
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_out,
		      mus_format("data format %d (%s) not available on %s",
				 format, 
				 mus_data_format_name(format), 
				 dev_name));
#ifdef NEW_OSS
  if (ioctl(audio_out, SNDCTL_DSP_CHANNELS, &chans) == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_out,
		      mus_format("can't get %d channels on %s",
				 chans, dev_name));
#else
  if (chans == 2) stereo = 1; else stereo = 0;
  if ((ioctl(audio_out, SNDCTL_DSP_STEREO, &stereo) == -1) || 
      ((chans == 2) && (stereo == 0)))
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_out,
		      mus_format("can't get %d channels on %s",
				 chans, dev_name));
#endif
  if (ioctl(audio_out, SNDCTL_DSP_SPEED, &srate) == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, audio_out,
		      mus_format("can't set srate of %s to %d",
				 dev_name, srate));
  /* http://www.4front-tech.com/pguide/audio.html says this order has to be followed */
  return(audio_out);
}

static int oss_mus_audio_write(int line, char *buf, int bytes)
{
  int err;
  if (line < 0) return(-1);
  errno = 0;
  err = write(line, buf, bytes);
  if (err != bytes)
    {
      if (errno != 0)
	RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			  mus_format("write error: %s", strerror(errno)));
      else RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			     mus_format("wrote %d bytes of requested %d", err, bytes));
    }
  return(MUS_NO_ERROR);
}

static int oss_mus_audio_close(int line)
{
  return(linux_audio_close(line));
}

static int oss_mus_audio_read(int line, char *buf, int bytes)
{
  int err;
  if (line < 0) return(-1);
  errno = 0;
  err = read(line, buf, bytes);
  if (err != bytes) 
    {
      if (errno != 0)
	RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			  mus_format("read error: %s", strerror(errno)));
      else RETURN_ERROR_EXIT(MUS_AUDIO_READ_ERROR, -1,
			     mus_format("read %d bytes of requested %d", err, bytes));
    }
  return(MUS_NO_ERROR);
}

static char *oss_unsrc(int srcbit)
{
  if (srcbit == 0)
    return(strdup("none"));
  else
    {
      bool need_and = false;
      char *buf;
      buf = (char *)CALLOC(PRINT_BUFFER_SIZE, sizeof(char));
      if (srcbit & SOUND_MASK_MIC) {need_and = true; strcat(buf, "mic");}
      if (srcbit & SOUND_MASK_LINE) {if (need_and) strcat(buf, " and "); need_and = true; strcat(buf, "line in");}
      if (srcbit & SOUND_MASK_LINE1) {if (need_and) strcat(buf, " and "); need_and = true; strcat(buf, "line1");}
      if (srcbit & SOUND_MASK_LINE2) {if (need_and) strcat(buf, " and "); need_and = true; strcat(buf, "line2");}
      if (srcbit & SOUND_MASK_LINE3) {if (need_and) strcat(buf, " and "); need_and = true; strcat(buf, "line3");}
      if (srcbit & SOUND_MASK_CD) {if (need_and) strcat(buf, " and "); need_and = true; strcat(buf, "cd");}
      return(buf);
    }
}

static int oss_mus_audio_open_input(int ur_dev, int srate, int chans, int format, int requested_size)
{
  /* dev can be MUS_AUDIO_DEFAULT or MUS_AUDIO_DUPLEX_DEFAULT as well as the obvious others */
  int audio_fd = -1, oss_format, buffer_info, sys, dev, srcbit, cursrc, err;
  bool adat_mode = false;
  char *dev_name;
#ifndef NEW_OSS
  int stereo;
#endif
  sys = MUS_AUDIO_SYSTEM(ur_dev);
  dev = MUS_AUDIO_DEVICE(ur_dev);
  oss_format = to_oss_format(format);
  if (oss_format == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d (%s) not available",
				 format, 
				 mus_data_format_name(format)));
  if (audio_type[sys] == SONORUS_STUDIO)
    {
      adat_mode = (audio_mode[sys] == 1);
      switch (dev)
	{
	case MUS_AUDIO_DEFAULT:
	  if (adat_mode)
	    audio_fd = open(dev_name = sonorus_name(sys, 11), O_RDONLY, 0);
	  else audio_fd = open(dev_name = sonorus_name(sys, 5), O_RDONLY, 0);
	  break;
	case MUS_AUDIO_ADAT_IN:
	  audio_fd = open(dev_name = sonorus_name(sys, 11), O_RDONLY, 0);
	  break;
	case MUS_AUDIO_AES_IN:
	  audio_fd = open(dev_name = sonorus_name(sys, 20), O_RDONLY, 0);
	  break;
	case MUS_AUDIO_SPDIF_IN:
	  audio_fd = open(dev_name = sonorus_name(sys, 5), O_RDONLY, 0);
	  break;
	default:
	  RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, -1,
			    mus_format("no %s device on Sonorus?",
				       mus_audio_device_name(dev)));
	  break;
	}
      if (audio_fd == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_NO_INPUT_AVAILABLE, -1,
			  mus_format("can't open %s (Sonorus device %s): %s",
				     dev_name, 
				     mus_audio_device_name(dev), 
				     strerror(errno)));
#ifdef NEW_OSS
      if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &chans) == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
			  mus_format("can't get %d channels on %s (Sonorus device %s)",
				     chans, dev_name, 
				     mus_audio_device_name(dev)));
#endif
      return(audio_fd);
    }

#if HAVE_SAM_9407
  if (audio_type[sys] == SAM9407_DSP)
    {
      char dname[LABEL_BUFFER_SIZE];
      mus_snprintf(dname, LABEL_BUFFER_SIZE, "/dev/sam%d_dsp", audio_dsp[sys]);
      audio_fd = open(dname, O_RDONLY);
      if(audio_fd == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, audio_fd,
			  mus_format("can't open input %s: %s",
				     dname, 
				     strerror(errno)));
      if ((ioctl(audio_fd, SNDCTL_DSP_SETFMT, &oss_format) == -1) || 
	  (oss_format != to_oss_format(format)))
	RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_fd,
			  mus_format("can't set %s format to %d (%s)",
				     dname, format, 
				     mus_data_format_name(format)));
      if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &chans) == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
			  mus_format("can't get %d channels on %s",
				     chans, dname));
      if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &srate) == -1)
	RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, audio_fd,
			  mus_format("can't set srate to %d on %s",
				     srate, dname));
      FRAGMENT_SIZE = 14;
      buffer_info = (FRAGMENTS << 16) | (FRAGMENT_SIZE);
      ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &buffer_info);
      return(audio_fd);
    }
#endif

  if (((dev == MUS_AUDIO_DEFAULT) || (dev == MUS_AUDIO_DUPLEX_DEFAULT)) && (sys == 0))
    audio_fd = linux_audio_open(dev_name = dac_name(sys, 0), 
				O_RDWR, 0, sys);
  else audio_fd = linux_audio_open(dev_name = dac_name(sys, (dev == MUS_AUDIO_AUX_INPUT) ? 1 : 0), 
				   O_RDONLY, 0, sys);
  if (audio_fd == -1)
    {
      if (dev == MUS_AUDIO_DUPLEX_DEFAULT)
	RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, -1,
		       mus_format("can't open %s (device %s): %s",
				  dev_name, mus_audio_device_name(dev), strerror(errno)));
      if ((audio_fd = linux_audio_open(dev_name = dac_name(sys, (dev == MUS_AUDIO_AUX_INPUT) ? 1 : 0), 
				       O_RDONLY, 0, sys)) == -1)
        {
          if ((errno == EACCES) || (errno == ENOENT))
	    RETURN_ERROR_EXIT(MUS_AUDIO_NO_READ_PERMISSION, -1,
			      mus_format("can't open %s (device %s): %s\n  to get input in Linux, we need read permission on /dev/dsp",
					 dev_name, 
					 mus_audio_device_name(dev), 
					 strerror(errno)));
          else RETURN_ERROR_EXIT(MUS_AUDIO_NO_INPUT_AVAILABLE, -1,
				 mus_format("can't open %s (device %s): %s",
					    dev_name, 
					    mus_audio_device_name(dev), 
					    strerror(errno)));
        }
    }
#ifdef SNDCTL_DSP_SETDUPLEX
  else 
    ioctl(audio_fd, SNDCTL_DSP_SETDUPLEX, &err); /* not always a no-op! */
#endif
  if (audio_type[sys] == RME_HAMMERFALL) return(audio_fd);
  if (audio_type[sys] == DELTA_66) return(audio_fd);
  /* need to make sure the desired recording source is active -- does this actually have any effect? */
  switch (dev)
    {
    case MUS_AUDIO_MICROPHONE: srcbit = SOUND_MASK_MIC;                   break;
    case MUS_AUDIO_LINE_IN:    srcbit = SOUND_MASK_LINE;                  break;
    case MUS_AUDIO_LINE1:      srcbit = SOUND_MASK_LINE1;                 break;
    case MUS_AUDIO_LINE2:      srcbit = SOUND_MASK_LINE2;                 break;
    case MUS_AUDIO_LINE3:      srcbit = SOUND_MASK_LINE3;                 break; /* also digital1..3 */
    case MUS_AUDIO_DUPLEX_DEFAULT: 
    case MUS_AUDIO_DEFAULT:    srcbit = SOUND_MASK_LINE | SOUND_MASK_MIC; break;
    case MUS_AUDIO_CD:         srcbit = SOUND_MASK_CD;                    break;
    default:                   srcbit = 0;                                break;
      /* other possibilities: synth, radio, phonein but these apparently bypass the mixer (no gains?) */
    }
  ioctl(audio_fd, MIXER_READ(SOUND_MIXER_RECSRC), &cursrc);
  srcbit = (srcbit | cursrc);
  ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &srcbit);
  ioctl(audio_fd, MIXER_READ(SOUND_MIXER_RECSRC), &cursrc);
  if (cursrc != srcbit)
    {
      char *str1, *str2;
      str1 = oss_unsrc(srcbit);
      str2 = oss_unsrc(cursrc);
      mus_print("weird: tried to set recorder source to %s, but got %s?", str1, str2);
      FREE(str1);
      FREE(str2);
    }
  if ((fragments_locked) && (requested_size != 0))
    {
      buffer_info = (FRAGMENTS << 16) | (FRAGMENT_SIZE);
      ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &buffer_info);
    }
  if ((ioctl(audio_fd, SNDCTL_DSP_SETFMT, &oss_format) == -1) ||
      (oss_format != to_oss_format(format)))
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_fd,
		      mus_format("can't set %s format to %d (%s)",
				 dev_name, format, 
				 mus_data_format_name(format)));
#ifdef NEW_OSS
  if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &chans) == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
		      mus_format("can't get %d channels on %s",
				 chans, dev_name));
#else
  if (chans == 2) stereo = 1; else stereo = 0;
  if ((ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1) || 
      ((chans == 2) && (stereo == 0))) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
		      mus_format("can't get %d channels on %s (%s)",
				 chans, dev_name, 
				 mus_audio_device_name(dev)));
#endif
  if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &srate) == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, audio_fd,
		      mus_format("can't set srate to %d on %s (%s)",
				 srate, dev_name, 
				 mus_audio_device_name(dev)));
  return(audio_fd);
}


static int oss_mus_audio_mixer_read(int ur_dev, int field, int chan, float *val)
{
  int fd, amp, channels, err = MUS_NO_ERROR, devmask, stereodevs, ind, formats, sys, dev, srate;
  char *dev_name = NULL;
  sys = MUS_AUDIO_SYSTEM(ur_dev);
  dev = MUS_AUDIO_DEVICE(ur_dev);
  if (audio_type[sys] == SONORUS_STUDIO)
    {
      bool adat_mode = false;
      adat_mode = (audio_mode[sys] == 1);
      if (dev == MUS_AUDIO_MIXER) val[0] = 0; /* no mixer */
      else
	{
	  if (field == MUS_AUDIO_PORT)
	    {
	      if (adat_mode)
		{
		  val[0] = 5;
		  val[1] = MUS_AUDIO_ADAT_IN;
		  val[2] = MUS_AUDIO_ADAT_OUT;
		  val[3] = MUS_AUDIO_SPEAKERS;
		  val[4] = MUS_AUDIO_AES_IN;
		  val[5] = MUS_AUDIO_AES_OUT;
		}
	      else
		{
		  val[0] = 3;
		  val[1] = MUS_AUDIO_SPDIF_IN;
		  val[2] = MUS_AUDIO_SPDIF_OUT;
		  val[3] = MUS_AUDIO_SPEAKERS;
		}
	    }
	  else
	    {
	      if (field == MUS_AUDIO_FORMAT)
		{
		  val[0] = 1;
		  val[1] = MUS_LSHORT;
		}
	      else
		{
		  if (field == MUS_AUDIO_CHANNEL)
		    {
		      switch (dev)
			{
			case MUS_AUDIO_SPEAKERS:
			  channels = 2; 
			  break;
			case MUS_AUDIO_ADAT_IN: case MUS_AUDIO_ADAT_OUT:
			  channels = 8; 
			  break;
			case MUS_AUDIO_AES_IN: case MUS_AUDIO_AES_OUT:
			  channels = 2; 
			  break;
			case MUS_AUDIO_SPDIF_IN: case MUS_AUDIO_SPDIF_OUT:
			  channels = 4; 
			  break;
			case MUS_AUDIO_DEFAULT: 
			  if (adat_mode) 
			    channels = 8; 
			  else channels = 4; 
			  break;
			default:
			  channels = 0; 
			  break;
			}
		      val[0] = channels;
		    }
		  else
		    {
		      if (field == MUS_AUDIO_SRATE)
			{
			  val[0] = 44100;
			}
		    }
		}
	    }
	}
      return(MUS_NO_ERROR);
    }

#if HAVE_SAM_9407
  if (audio_type[sys] == SAM9407_DSP)
    {
      switch(field) 
	{
	case MUS_AUDIO_PORT:
	  val[0] = 2;
	  val[1] = MUS_AUDIO_SPEAKERS;
	  val[2] = MUS_AUDIO_LINE_IN;
	  break;
	case MUS_AUDIO_FORMAT:
	  val[0] = 1;
	  val[1] = MUS_LSHORT;
	  break;
	case MUS_AUDIO_CHANNEL:
	  val[0] = 2;
	  break;
	case MUS_AUDIO_AMP:
	  RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, -1,
			    mus_format("can't read %s's gains in Sam9407",
				       mus_audio_device_name(dev)));
	  break;
	case MUS_AUDIO_SRATE:
	  val[0] = 44100;
	  break;
	default:
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1,
			    mus_format("can't read %s's %s in Sam9407",
				       mus_audio_device_name(dev),
				       mus_audio_device_name(field)));
	  break;
	}
      return(MUS_NO_ERROR);
    }
#endif

  if (audio_type[sys] == RME_HAMMERFALL)
    {
      if (dev == MUS_AUDIO_MIXER) val[0] = 0; /* no mixer */
      else
	{
	  if (field == MUS_AUDIO_PORT)
	    {
	      val[0] = 5;
	      val[1] = MUS_AUDIO_ADAT_IN;
	      val[2] = MUS_AUDIO_ADAT_OUT;
	      val[3] = MUS_AUDIO_SPEAKERS;
	      val[4] = MUS_AUDIO_AES_IN;
	      val[5] = MUS_AUDIO_AES_OUT;
	    }
	  else
	    {
	      if (field == MUS_AUDIO_FORMAT)
		{
		  val[0] = 1;
		  val[1] = MUS_LSHORT;
		}
	      else
		{
		  if (field == MUS_AUDIO_CHANNEL)
		    {
		      switch (dev)
			{
			case MUS_AUDIO_SPEAKERS:
			  channels = 2; 
			  break;
			case MUS_AUDIO_ADAT_IN: case MUS_AUDIO_ADAT_OUT:
			  channels = 8; 
			  break;
			case MUS_AUDIO_AES_IN: case MUS_AUDIO_AES_OUT:
			  channels = 2; 
			  break;
			case MUS_AUDIO_SPDIF_IN: case MUS_AUDIO_SPDIF_OUT:
			  channels = 4; 
			  break;
			case MUS_AUDIO_DEFAULT:
			  channels = 8; 
			  break;
			default:
			  channels = 0;
			  break;
			}
		      val[0] = channels;
		    }
		  else
		    {
		      if (field == MUS_AUDIO_SRATE)
			{
			  val[0] = 44100;
			}
		    }
		}
	    }
	}
      return(MUS_NO_ERROR);
    }

  fd = linux_audio_open(dev_name = mixer_name(sys), O_RDONLY | O_NONBLOCK, 0, sys);
  if (fd == -1) 
    {
      fd = linux_audio_open(DAC_NAME, O_RDONLY, 0, sys);
      if (fd == -1)
	{
	  fd = linux_audio_open(DAC_NAME, O_WRONLY, 0, sys);
	  if (fd == -1) 
	    {
	      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, -1,
				mus_format("can't open input %s or %s: %s",
					   dev_name, DAC_NAME, 
					   strerror(errno)));
	      return(MUS_ERROR);
	    }
	  else dev_name = DAC_NAME;
	}
      else dev_name = DAC_NAME;
    }
  if (ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask))
    RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, fd,
		      mus_format("can't read device info from %s",
				 dev_name));
  err = 0;
  if ((dev == MUS_AUDIO_MIXER) || 
      (dev == MUS_AUDIO_DAC_FILTER))           /* these give access to all the on-board analog input gain controls */
    {
      amp = 0;
      ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask);
      switch (field)
        { 
          /* also DIGITAL1..3 PHONEIN PHONEOUT VIDEO RADIO MONITOR */
	  /* the digital lines should get their own panes in the recorder */
	  /* not clear whether the phone et al lines are routed to the ADC */
	  /* also, I've never seen a card with any of these devices */
        case MUS_AUDIO_IMIX:   if (SOUND_MASK_IMIX & devmask)    err = ioctl(fd, MIXER_READ(SOUND_MIXER_IMIX), &amp);     break;
        case MUS_AUDIO_IGAIN:  if (SOUND_MASK_IGAIN & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_IGAIN), &amp);    break;
        case MUS_AUDIO_RECLEV: if (SOUND_MASK_RECLEV & devmask)  err = ioctl(fd, MIXER_READ(SOUND_MIXER_RECLEV), &amp);   break;
        case MUS_AUDIO_PCM:    if (SOUND_MASK_PCM & devmask)     err = ioctl(fd, MIXER_READ(SOUND_MIXER_PCM), &amp);      break;
        case MUS_AUDIO_PCM2:   if (SOUND_MASK_ALTPCM & devmask)  err = ioctl(fd, MIXER_READ(SOUND_MIXER_ALTPCM), &amp);   break;
        case MUS_AUDIO_OGAIN:  if (SOUND_MASK_OGAIN & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_OGAIN), &amp);    break;
        case MUS_AUDIO_LINE:   if (SOUND_MASK_LINE & devmask)    err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE), &amp);     break;
        case MUS_AUDIO_MICROPHONE: if (SOUND_MASK_MIC & devmask) err = ioctl(fd, MIXER_READ(SOUND_MIXER_MIC), &amp);      break;
        case MUS_AUDIO_LINE1:  if (SOUND_MASK_LINE1 & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE1), &amp);    break;
        case MUS_AUDIO_LINE2:  if (SOUND_MASK_LINE2 & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE2), &amp);    break;
        case MUS_AUDIO_LINE3:  if (SOUND_MASK_LINE3 & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE3), &amp);    break;
        case MUS_AUDIO_SYNTH:  if (SOUND_MASK_SYNTH & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_SYNTH), &amp);    break;
        case MUS_AUDIO_BASS:   if (SOUND_MASK_BASS & devmask)    err = ioctl(fd, MIXER_READ(SOUND_MIXER_BASS), &amp);     break;
        case MUS_AUDIO_TREBLE: if (SOUND_MASK_TREBLE & devmask)  err = ioctl(fd, MIXER_READ(SOUND_MIXER_TREBLE), &amp);   break;
        case MUS_AUDIO_CD:     if (SOUND_MASK_CD & devmask)      err = ioctl(fd, MIXER_READ(SOUND_MIXER_CD), &amp);       break;
        case MUS_AUDIO_CHANNEL:
          if (dev == MUS_AUDIO_MIXER)
            {
              channels = 0;
              ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereodevs);
	      if (SOUND_MASK_IMIX & devmask)   {if (SOUND_MASK_IMIX & stereodevs)   channels += 2; else channels += 1;}
	      if (SOUND_MASK_IGAIN & devmask)  {if (SOUND_MASK_IGAIN & stereodevs)  channels += 2; else channels += 1;}
	      if (SOUND_MASK_RECLEV & devmask) {if (SOUND_MASK_RECLEV & stereodevs) channels += 2; else channels += 1;}
	      if (SOUND_MASK_PCM & devmask)    {if (SOUND_MASK_PCM & stereodevs)    channels += 2; else channels += 1;}
	      if (SOUND_MASK_ALTPCM & devmask) {if (SOUND_MASK_ALTPCM & stereodevs) channels += 2; else channels += 1;}
	      if (SOUND_MASK_OGAIN & devmask)  {if (SOUND_MASK_OGAIN & stereodevs)  channels += 2; else channels += 1;}
	      if (SOUND_MASK_LINE & devmask)   {if (SOUND_MASK_LINE & stereodevs)   channels += 2; else channels += 1;}
	      if (SOUND_MASK_MIC & devmask)    {if (SOUND_MASK_MIC & stereodevs)    channels += 2; else channels += 1;}
	      if (SOUND_MASK_LINE1 & devmask)  {if (SOUND_MASK_LINE1 & stereodevs)  channels += 2; else channels += 1;}
	      if (SOUND_MASK_LINE2 & devmask)  {if (SOUND_MASK_LINE2 & stereodevs)  channels += 2; else channels += 1;}
	      if (SOUND_MASK_LINE3 & devmask)  {if (SOUND_MASK_LINE3 & stereodevs)  channels += 2; else channels += 1;}
	      if (SOUND_MASK_SYNTH & devmask)  {if (SOUND_MASK_SYNTH & stereodevs)  channels += 2; else channels += 1;}
	      if (SOUND_MASK_CD & devmask)     {if (SOUND_MASK_CD & stereodevs)     channels += 2; else channels += 1;}
            }
          else 
            if (SOUND_MASK_TREBLE & devmask) channels = 2; else channels = 0;
          val[0] = channels;
          linux_audio_close(fd);
          return(MUS_NO_ERROR);
          break;
        case MUS_AUDIO_FORMAT: /* this is asking for configuration info -- we return an array with per-"device" channels */
          ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereodevs);
	  for (ind = 0; ind <= MUS_AUDIO_SYNTH; ind++) {if (chan > ind) val[ind] = 0;}
	  if (SOUND_MASK_IMIX & devmask)   {if (chan > MUS_AUDIO_IMIX)       val[MUS_AUDIO_IMIX] = ((SOUND_MASK_IMIX & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_IGAIN & devmask)  {if (chan > MUS_AUDIO_IGAIN)      val[MUS_AUDIO_IGAIN] = ((SOUND_MASK_IGAIN & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_RECLEV & devmask) {if (chan > MUS_AUDIO_RECLEV)     val[MUS_AUDIO_RECLEV] = ((SOUND_MASK_RECLEV & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_PCM & devmask)    {if (chan > MUS_AUDIO_PCM)        val[MUS_AUDIO_PCM] = ((SOUND_MASK_PCM & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_ALTPCM & devmask) {if (chan > MUS_AUDIO_PCM2)       val[MUS_AUDIO_PCM2] = ((SOUND_MASK_ALTPCM & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_OGAIN & devmask)  {if (chan > MUS_AUDIO_OGAIN)      val[MUS_AUDIO_OGAIN] = ((SOUND_MASK_OGAIN & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_LINE & devmask)   {if (chan > MUS_AUDIO_LINE)       val[MUS_AUDIO_LINE] = ((SOUND_MASK_LINE & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_MIC & devmask)    {if (chan > MUS_AUDIO_MICROPHONE) val[MUS_AUDIO_MICROPHONE] = ((SOUND_MASK_MIC & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_LINE1 & devmask)  {if (chan > MUS_AUDIO_LINE1)      val[MUS_AUDIO_LINE1] = ((SOUND_MASK_LINE1 & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_LINE2 & devmask)  {if (chan > MUS_AUDIO_LINE2)      val[MUS_AUDIO_LINE2] = ((SOUND_MASK_LINE2 & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_LINE3 & devmask)  {if (chan > MUS_AUDIO_LINE3)      val[MUS_AUDIO_LINE3] = ((SOUND_MASK_LINE3 & stereodevs) ? 2 : 1);}
 	  if (SOUND_MASK_SYNTH & devmask)  {if (chan > MUS_AUDIO_SYNTH)      val[MUS_AUDIO_SYNTH] = ((SOUND_MASK_SYNTH & stereodevs) ? 2 : 1);}
	  if (SOUND_MASK_CD & devmask)     {if (chan > MUS_AUDIO_CD)         val[MUS_AUDIO_CD] = ((SOUND_MASK_CD & stereodevs) ? 2 : 1);}
          linux_audio_close(fd);
          return(MUS_NO_ERROR);
          break;
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, fd,
			    mus_format("can't read %s's (%s) %s",
				       mus_audio_device_name(dev), dev_name,
				       mus_audio_device_name(field)));
	  break;
        }
      if (chan == 0)
        val[0] = ((float)(amp & 0xff)) * 0.01;
      else val[0] = (((float)((amp & 0xff00) >> 8)) * 0.01);
    }
  else
    {
      switch (field)
        {
        case MUS_AUDIO_PORT:
          ind = 1;
          val[1] = MUS_AUDIO_MIXER;
          if ((SOUND_MASK_MIC | SOUND_MASK_LINE | SOUND_MASK_CD) & devmask) {ind++; if (chan > ind) val[ind] = MUS_AUDIO_LINE_IN;}
          /* problem here is that microphone and line_in are mixed before the ADC */
          if (SOUND_MASK_SPEAKER & devmask) {ind++; if (chan > ind) val[ind] = MUS_AUDIO_SPEAKERS;}
          if (SOUND_MASK_VOLUME & devmask)  {ind++; if (chan > ind) val[ind] = MUS_AUDIO_DAC_OUT;}
          if (SOUND_MASK_TREBLE & devmask)  {ind++; if (chan > ind) val[ind] = MUS_AUDIO_DAC_FILTER;}
          /* DIGITAL1..3 as RECSRC(?) => MUS_AUDIO_DIGITAL_IN */
          val[0] = ind;
          break;
#if 1
         case MUS_AUDIO_FORMAT:
	  linux_audio_close(fd);
	  fd = open(dac_name(sys, 0), O_WRONLY, 0);
	  if (fd == -1) fd = open(DAC_NAME, O_WRONLY, 0);
	  if (fd == -1) 
	    {
	      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, -1,
				mus_format("can't open %s: %s",
					   DAC_NAME, strerror(errno)));
	      return(MUS_ERROR);
	    }
           ioctl(fd, SOUND_PCM_GETFMTS, &formats);
#else
        case MUS_AUDIO_FORMAT:
          ioctl(fd, SOUND_PCM_GETFMTS, &formats);
	  /* this returns -1 and garbage?? */

	  /* from Steven Schultz:
	I did discover why, in audio.c the SOUND_PCM_GETFMTS ioctl was failing.
	That ioctl call can only be made against the /dev/dsp device and _not_
	the /dev/mixer device.  With that change things starting working real
	nice.
	  */
#endif
	  ind = 0;
	  if (formats & (to_oss_format(MUS_BYTE)))    {ind++; if (chan > ind) val[ind] = MUS_BYTE;}
 	  if (formats & (to_oss_format(MUS_BSHORT)))  {ind++; if (chan > ind) val[ind] = MUS_BSHORT;}
	  if (formats & (to_oss_format(MUS_UBYTE)))   {ind++; if (chan > ind) val[ind] = MUS_UBYTE;}
	  if (formats & (to_oss_format(MUS_MULAW)))   {ind++; if (chan > ind) val[ind] = MUS_MULAW;}
	  if (formats & (to_oss_format(MUS_ALAW)))    {ind++; if (chan > ind) val[ind] = MUS_ALAW;}
	  if (formats & (to_oss_format(MUS_LSHORT)))  {ind++; if (chan > ind) val[ind] = MUS_LSHORT;}
	  if (formats & (to_oss_format(MUS_UBSHORT))) {ind++; if (chan > ind) val[ind] = MUS_UBSHORT;}
	  if (formats & (to_oss_format(MUS_ULSHORT))) {ind++; if (chan > ind) val[ind] = MUS_ULSHORT;}
	  val[0] = ind;
          break;
        case MUS_AUDIO_CHANNEL:
	  channels = 0;
          ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereodevs);
	  switch (dev)
	    {
	    case MUS_AUDIO_MICROPHONE: if (SOUND_MASK_MIC & devmask)     {if (SOUND_MASK_MIC & stereodevs) channels = 2; else channels = 1;}     break;
	    case MUS_AUDIO_SPEAKERS:   if (SOUND_MASK_SPEAKER & devmask) {if (SOUND_MASK_SPEAKER & stereodevs) channels = 2; else channels = 1;} break;
	    case MUS_AUDIO_LINE_IN:    if (SOUND_MASK_LINE & devmask)    {if (SOUND_MASK_LINE & stereodevs) channels = 2; else channels = 1;}    break;
	    case MUS_AUDIO_LINE1:      if (SOUND_MASK_LINE1 & devmask)   {if (SOUND_MASK_LINE1 & stereodevs) channels = 2; else channels = 1;}   break;
	    case MUS_AUDIO_LINE2:      if (SOUND_MASK_LINE2 & devmask)   {if (SOUND_MASK_LINE2 & stereodevs) channels = 2; else channels = 1;}   break;
	    case MUS_AUDIO_LINE3:      if (SOUND_MASK_LINE3 & devmask)   {if (SOUND_MASK_LINE3 & stereodevs) channels = 2; else channels = 1;}   break;
	    case MUS_AUDIO_DAC_OUT:    if (SOUND_MASK_VOLUME & devmask)  {if (SOUND_MASK_VOLUME & stereodevs) channels = 2; else channels = 1;}  break;
	    case MUS_AUDIO_DEFAULT:    if (SOUND_MASK_VOLUME & devmask)  {if (SOUND_MASK_VOLUME & stereodevs) channels = 2; else channels = 1;}  break;
	    case MUS_AUDIO_CD:         if (SOUND_MASK_CD & devmask)      {if (SOUND_MASK_CD & stereodevs) channels = 2; else channels = 1;}      break;
	    case MUS_AUDIO_DUPLEX_DEFAULT: 
	      err = ioctl(fd, SNDCTL_DSP_GETCAPS, &ind);
	      if (err != -1)
		channels = (ind & DSP_CAP_DUPLEX);
	      else channels = 0;
	      break;
	    default: 
	      RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, fd,
				mus_format("can't read channel info from %s (%s)",
					   mus_audio_device_name(dev), dev_name));
	      break;
            }
          val[0] = channels;
          break;
        case MUS_AUDIO_AMP:
          amp = 0;
          switch (dev)
            {
            case MUS_AUDIO_MICROPHONE: if (SOUND_MASK_MIC & devmask)     err = ioctl(fd, MIXER_READ(SOUND_MIXER_MIC), &amp);     break;
            case MUS_AUDIO_SPEAKERS:   if (SOUND_MASK_SPEAKER & devmask) err = ioctl(fd, MIXER_READ(SOUND_MIXER_SPEAKER), &amp); break; 
            case MUS_AUDIO_LINE_IN:    if (SOUND_MASK_LINE & devmask)    err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE), &amp);    break; 
            case MUS_AUDIO_LINE1:      if (SOUND_MASK_LINE1 & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE1), &amp);   break; 
            case MUS_AUDIO_LINE2:      if (SOUND_MASK_LINE2 & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE2), &amp);   break; 
            case MUS_AUDIO_LINE3:      if (SOUND_MASK_LINE3 & devmask)   err = ioctl(fd, MIXER_READ(SOUND_MIXER_LINE3), &amp);   break; 
            case MUS_AUDIO_DAC_OUT:    if (SOUND_MASK_VOLUME & devmask)  err = ioctl(fd, MIXER_READ(SOUND_MIXER_VOLUME), &amp);  break;
            case MUS_AUDIO_DEFAULT:    if (SOUND_MASK_VOLUME & devmask)  err = ioctl(fd, MIXER_READ(SOUND_MIXER_VOLUME), &amp);  break;
            case MUS_AUDIO_CD:         if (SOUND_MASK_CD & devmask)      err = ioctl(fd, MIXER_READ(SOUND_MIXER_CD), &amp);      break;
            default: 
	      RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, fd,
				mus_format("can't get gain info for %s (%s)",
					   mus_audio_device_name(dev), dev_name));
	      break;
            }
	  if (err) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, fd,
			      mus_format("can't read %s's (%s) amp info",
					   mus_audio_device_name(dev), dev_name));
          if (chan == 0)
            val[0] = ((float)(amp & 0xff)) * 0.01;
          else val[0] = (((float)((amp & 0xff00) >> 8)) * 0.01);
          break;
	case MUS_AUDIO_SRATE:
	  srate = (int)(val[0]);
	  if (ioctl(fd, SNDCTL_DSP_SPEED, &srate) == -1) 
	    {
	      linux_audio_close(fd);
	      /* see comment from Steven Schultz above */
	      fd = open(dac_name(sys, 0), O_WRONLY, 0);
	      if (fd == -1) fd = open(DAC_NAME, O_WRONLY, 0);
	      if (ioctl(fd, SNDCTL_DSP_SPEED, &srate) == -1) 
		RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, fd,
				  mus_format("can't get %s's (%s) srate",
					     mus_audio_device_name(dev), dev_name));
	    }
	  val[0] = (float)srate;
	  break;
	case MUS_AUDIO_DIRECTION:
	  switch (dev)
	    {
	    case MUS_AUDIO_DIGITAL_OUT: case MUS_AUDIO_LINE_OUT: case MUS_AUDIO_DEFAULT:  case MUS_AUDIO_ADAT_OUT:
	    case MUS_AUDIO_AES_OUT: case MUS_AUDIO_SPDIF_OUT: case MUS_AUDIO_SPEAKERS: case MUS_AUDIO_MIXER:
	    case MUS_AUDIO_DAC_FILTER:  case MUS_AUDIO_AUX_OUTPUT: case MUS_AUDIO_DAC_OUT: 
	      val[0] = 0.0;
	      break;
	    default:  
	      val[0] = 1.0; 
	      break;
	    }
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, fd,
			    mus_format("can't get %s's (%s) %s",
				       mus_audio_device_name(dev), dev_name,
				       mus_audio_device_name(field)));
	  break;
        }
    }
  return(linux_audio_close(fd));
}

static int oss_mus_audio_mixer_write(int ur_dev, int field, int chan, float *val)
{
  int fd, err = MUS_NO_ERROR, devmask, vol, sys, dev;
  char *dev_name;
  float amp[1];
  sys = MUS_AUDIO_SYSTEM(ur_dev);
  dev = MUS_AUDIO_DEVICE(ur_dev);

#if HAVE_SAM_9407
  if (audio_type[sys] == SAM9407_DSP) return(MUS_NO_ERROR); /* XXX */
#endif

  if (audio_type[sys] == SONORUS_STUDIO) return(MUS_NO_ERROR); /* there are apparently volume controls, but they're not accessible yet */
  if (audio_type[sys] == RME_HAMMERFALL) return(MUS_NO_ERROR);
  if (audio_type[sys] == DELTA_66) return(MUS_NO_ERROR);

  fd = linux_audio_open(dev_name = mixer_name(sys), O_RDWR | O_NONBLOCK, 0, sys);
  if (fd == -1) 
    {
      fd = linux_audio_open_with_error(dev_name = DAC_NAME, O_WRONLY, 0, sys);
      if (fd == -1) return(MUS_ERROR);
    }
  if ((dev == MUS_AUDIO_MIXER) || 
      (dev == MUS_AUDIO_DAC_FILTER))                          /* these give access to all the on-board analog input gain controls */
    {
      if (mus_audio_mixer_read(ur_dev, field, (chan == 0) ? 1 : 0, amp)) 
	{
	  linux_audio_close(fd); 
	  return(MUS_ERROR);
	}
      if (val[0] >= 0.99) val[0] = 0.99;  
      if (val[0] < 0.0) val[0] = 0.0;
      if (amp[0] >= 0.99) amp[0] = 0.99;
      if (chan == 0)
        vol = (((int)(amp[0] * 100)) << 8) + ((int)(val[0] * 100));
      else vol = (((int)(val[0] * 100)) << 8) + ((int)(amp[0] * 100));
      ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask);
      switch (field)
        {
        case MUS_AUDIO_IMIX:   if (SOUND_MASK_IMIX & devmask)    err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_IMIX), &vol);    break;
        case MUS_AUDIO_IGAIN:  if (SOUND_MASK_IGAIN & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_IGAIN), &vol);   break;
        case MUS_AUDIO_RECLEV: if (SOUND_MASK_RECLEV & devmask)  err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_RECLEV), &vol);  break;
        case MUS_AUDIO_PCM:    if (SOUND_MASK_PCM & devmask)     err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_PCM), &vol);     break;
        case MUS_AUDIO_PCM2:   if (SOUND_MASK_ALTPCM & devmask)  err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_ALTPCM), &vol);  break;
        case MUS_AUDIO_OGAIN:  if (SOUND_MASK_OGAIN & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_OGAIN), &vol);   break;
        case MUS_AUDIO_LINE:   if (SOUND_MASK_LINE & devmask)    err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE), &vol);    break;
        case MUS_AUDIO_MICROPHONE: if (SOUND_MASK_MIC & devmask) err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_MIC), &vol);     break;
        case MUS_AUDIO_LINE1:  if (SOUND_MASK_LINE1 & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE1), &vol);   break;
        case MUS_AUDIO_LINE2:  if (SOUND_MASK_LINE2 & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE2), &vol);   break;
        case MUS_AUDIO_LINE3:  if (SOUND_MASK_LINE3 & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE3), &vol);   break;
        case MUS_AUDIO_SYNTH:  if (SOUND_MASK_SYNTH & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_SYNTH), &vol);   break;
        case MUS_AUDIO_BASS:   if (SOUND_MASK_BASS & devmask)    err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_BASS), &vol);    break;
        case MUS_AUDIO_TREBLE: if (SOUND_MASK_TREBLE & devmask)  err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_TREBLE), &vol);  break;
        case MUS_AUDIO_CD:     if (SOUND_MASK_CD & devmask)      err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_CD), &vol);      break;
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, fd,
			    mus_format("can't write %s's (%s) %s field",
				       mus_audio_device_name(dev), dev_name,
				       mus_audio_device_name(field)));
	  break;
        }
    }
  else
    {
      switch (field)
        {
        case MUS_AUDIO_AMP:
          /* need to read both channel amps, then change the one we're concerned with */
          mus_audio_mixer_read(ur_dev, field, (chan == 0) ? 1 : 0, amp);
          if (val[0] >= 0.99) val[0] = 0.99;  
	  if (val[0] < 0.0) val[0] = 0.0;
          if (amp[0] >= 0.99) amp[0] = 0.99;
          if (chan == 0)
            vol = (((int)(amp[0] * 100)) << 8) + ((int)(val[0] * 100));
          else vol = (((int)(val[0] * 100)) << 8) + ((int)(amp[0] * 100));
          ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask);
          switch (dev)
            {
            case MUS_AUDIO_MICROPHONE: if (SOUND_MASK_MIC & devmask)     err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_MIC), &vol);     break;
            case MUS_AUDIO_SPEAKERS:   if (SOUND_MASK_SPEAKER & devmask) err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_SPEAKER), &vol); break;
            case MUS_AUDIO_LINE_IN:    if (SOUND_MASK_LINE & devmask)    err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE), &vol);    break;
            case MUS_AUDIO_LINE1:      if (SOUND_MASK_LINE1 & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE1), &vol);   break;
            case MUS_AUDIO_LINE2:      if (SOUND_MASK_LINE2 & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE2), &vol);   break;
            case MUS_AUDIO_LINE3:      if (SOUND_MASK_LINE3 & devmask)   err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE3), &vol);   break;
            case MUS_AUDIO_DAC_OUT:    if (SOUND_MASK_VOLUME & devmask)  err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &vol);  break;
            case MUS_AUDIO_DEFAULT:    if (SOUND_MASK_VOLUME & devmask)  err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &vol);  break;
            case MUS_AUDIO_CD:         if (SOUND_MASK_CD & devmask)      err = ioctl(fd, MIXER_WRITE(SOUND_MIXER_CD), &vol);      break;
            default: 
	      RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, fd,
				mus_format("device %d (%s) not available on %s",
					   dev, mus_audio_device_name(dev), dev_name));
            }
          break;
        case MUS_AUDIO_SRATE:
          vol = (int)val[0];
	  linux_audio_close(fd);
	  /* see comment from Steven Schultz above */
	  fd = open(dac_name(sys, 0), O_WRONLY | O_NONBLOCK, 0);
	  if (fd == -1) 
	    {
	      fd = open(DAC_NAME, O_WRONLY | O_NONBLOCK, 0);
	      if (fd == -1) return(-1);
	    }
          err = ioctl(fd, SNDCTL_DSP_SPEED, &vol);
          break;
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, fd,
			    mus_format("can't write %s's (%s) %s field",
				       mus_audio_device_name(dev), dev_name,
				       mus_audio_device_name(field)));
	  break;
          /* case MUS_AUDIO_FORMAT: to force 16-bit input or give up */
          /* case MUS_AUDIO_CHANNEL: to open as stereo if possible?? */
          /* case MUS_AUDIO_PORT: to open digital out? */
        }
    }
  if (err) 
    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, fd,
		      mus_format("possible write problem for %s's (%s) %s field",
				 mus_audio_device_name(dev), dev_name,
				 mus_audio_device_name(field)));
  return(linux_audio_close(fd));
}

static char *synth_names[] = 
  {"",
   "Adlib", "SoundBlaster", "ProAudio Spectrum", "Gravis UltraSound", "MPU 401",
   "SoundBlaster 16", "SoundBlaster 16 MIDI", "6850 UART", "Gravis UltraSound 16", "Microsoft",
   "Personal sound system", "Ensoniq Soundscape", "Personal sound system + MPU", "Personal/Microsoft",
   "Mediatrix Pro", "MAD16", "MAD16 + MPU", "CS4232", "CS4232 + MPU", "Maui",
   "Pseudo-MSS", "Gravis Ultrasound PnP", "UART 401"};

static char *synth_name(int i)
{
#ifdef SNDCARD_UART401
  if ((i > 0) && (i <= SNDCARD_UART401)) 
#else
  if ((i > 0) && (i <= 26))
#endif
    return(synth_names[i]);
  return("unknown");
}

static char *device_types[] = {"FM", "Sampling", "MIDI"};

static char *device_type(int i)
{
  if ((i >= 0) && (i <= 2))
    return(device_types[i]);
  return("unknown");
}

static void yes_no(int condition)
{
  if (condition)  
    pprint("   yes    ");
  else pprint("   no     "); 
}

static int set_dsp(int fd, int channels, int bits, int *rate)
{
  int val;
  val = channels;
  ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &val);
  if (val != channels) return(MUS_ERROR);
  val = bits;
  ioctl(fd, SOUND_PCM_WRITE_BITS, &val);
  if (val != bits) return(MUS_ERROR);
  ioctl(fd, SOUND_PCM_WRITE_RATE, rate);
  return(MUS_NO_ERROR);
}

static void oss_describe_audio_state_1(void)
{
  /* this code taken largely from "Linux Multimedia Guide" by Jeff Tranter, O'Reilly & Associates, Inc 1996 */
  /* it is explicitly released under the GPL, so I think I can use it here without elaborate disguises */
  int fd;
  int status = 0, level, i, recsrc, devmask, recmask, stereodevs, caps;
  int numdevs = 0, rate = 0, channels = 0, bits = 0, blocksize = 0, formats = 0, deffmt = 0, min_rate = 0, max_rate = 0;
  struct synth_info sinfo;
  struct midi_info minfo;
  const char *sound_device_names[] = SOUND_DEVICE_LABELS;
  char dsp_name[LABEL_BUFFER_SIZE];
  char version[LABEL_BUFFER_SIZE];
  int dsp_num = 0;
#ifdef NEW_OSS
  mixer_info mixinfo;
  oss_sysinfo sysinfo;
#endif
  
  if (sound_cards <= 0) mus_audio_initialize();
  memset((void *)dsp_name, 0, LABEL_BUFFER_SIZE);
  memset((void *)version, 0, LABEL_BUFFER_SIZE);

#ifdef NEW_OSS
  fd = open(DAC_NAME, O_WRONLY, 0);
  if (fd == -1) fd = open(SYNTH_NAME, O_RDONLY, 0);
  if (fd == -1) fd = open(MIXER_NAME, O_RDONLY, 0);
  if (fd != -1)
    {
      status = ioctl(fd, OSS_GETVERSION, &level);
      new_oss_running = (status == 0);
      status = ioctl(fd, OSS_SYSINFO, &sysinfo);
      close(fd);
    }
#endif

  if (new_oss_running)
    {
#ifdef NEW_OSS
      if (status == 0)
	{
	  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "OSS version: %s\n", sysinfo.version);
	  pprint(audio_strbuf);
	}
      else
	{
	  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "OSS version: %x.%x.%x\n", (level >> 16) & 0xff, (level >> 8) & 0xff, level & 0xff);
	  pprint(audio_strbuf);
	}
#else
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "OSS version: %x.%x.%x\n", (level >> 16) & 0xff, (level >> 8) & 0xff, level & 0xff);
      pprint(audio_strbuf);
#endif
    }
  else 
    {
      /* refers to the version upon compilation */
      mus_snprintf(version, LABEL_BUFFER_SIZE, "%d", SOUND_VERSION);
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "OSS version: %c.%c.%c\n", version[0], version[1], version[2]);
      pprint(audio_strbuf);
    }
  
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%d card%s found", sound_cards, (sound_cards != 1) ? "s" : ""); pprint(audio_strbuf);
  if (sound_cards > 1)
    {
      pprint(": ");
      for (i = 0; i < sound_cards; i++)
	{
	  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "/dev/dsp%d with /dev/mixer%d%s", 
		       audio_dsp[i], 
		       audio_mixer[i], 
		       (i < (sound_cards - 1)) ? ", " : "");
	  pprint(audio_strbuf);
	}
    }
  pprint("\n\n");

  fd = open(SYNTH_NAME, O_RDWR, 0);
  if (fd == -1) fd = open(SYNTH_NAME, O_RDONLY, 0);
  if (fd == -1) 
    {
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s: %s\n", SYNTH_NAME, strerror(errno)); pprint(audio_strbuf); 
      pprint("no synth found\n"); 
    }
  else
    {
      status = ioctl(fd, SNDCTL_SEQ_NRSYNTHS, &numdevs);
      if (status == -1) 
	{
	  close(fd); fd = -1;
	  pprint("no sequencer?");
	}
      else
	{
	  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "/dev/sequencer: %d device%s installed\n", numdevs, (numdevs == 1) ? "" : "s"); 
	  pprint(audio_strbuf);
	  for (i = 0; i < numdevs; i++)
	    {
	      sinfo.device = i;
	      status = ioctl(fd, SNDCTL_SYNTH_INFO, &sinfo);
	      if (status != -1)
		{
		  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  device: %d: %s, %s, %d voices\n", i, sinfo.name, device_type(sinfo.synth_type), sinfo.nr_voices); 
		  pprint(audio_strbuf);
		}
	    }
	  status = ioctl(fd, SNDCTL_SEQ_NRMIDIS, &numdevs);
	  if (status == -1) 
	    {
	      close(fd); fd = -1;
	      pprint("no midi");
	    }
	  else
	    {
	      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %d midi device%s installed\n", numdevs, (numdevs == 1) ? "" : "s"); 
	      pprint(audio_strbuf);
	      for (i = 0; i < numdevs; i++)
		{
		  minfo.device = i;
		  status = ioctl(fd, SNDCTL_MIDI_INFO, &minfo);
		  if (status != -1)
		    {
		      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  device %d: %s, %s\n", i, minfo.name, synth_name(minfo.dev_type)); 
		      pprint(audio_strbuf);
		    }
		}
	    }
	}
    }
  if (fd != -1) close(fd);
  pprint("--------------------------------\n");

MIXER_INFO:
  mus_snprintf(dsp_name, LABEL_BUFFER_SIZE, "%s%d", MIXER_NAME, dsp_num);
  fd = linux_audio_open(dsp_name, O_RDWR, 0, 0);
  if (fd == -1) 
    {
      /* maybe output only */
      fd = linux_audio_open(dsp_name, O_WRONLY, 0, 0);
      if (fd == -1)
        {
          if (dsp_num == 0) 
	    {
	      mus_snprintf(dsp_name, LABEL_BUFFER_SIZE, "%s", DAC_NAME);
	      fd = linux_audio_open(DAC_NAME, O_RDWR, 0, 0);
	      if (fd == -1) 
		{
		  /* maybe output only */
		  fd = linux_audio_open(DAC_NAME, O_WRONLY, 0, 0);
		  if (fd == -1)
		    {
		      pprint("no audio device found\n"); 
		      return;
		    }
		}
	    }
	  else goto AUDIO_INFO; /* no /dev/mixern */
        }
      else pprint("no audio input enabled\n"); 
    }
  if (fd == -1) goto AUDIO_INFO;

#ifdef NEW_OSS
  if (new_oss_running) status = ioctl(fd, SOUND_MIXER_INFO, &mixinfo);
#endif
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s", dsp_name);
  pprint(audio_strbuf);
#ifdef NEW_OSS
  if ((new_oss_running) && (status == 0)) 
    {
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " (%s", mixinfo.name);
      pprint(audio_strbuf);
      for (i = 0; i < sound_cards; i++)
	{
	  if ((audio_mixer[i] == dsp_num) && (audio_type[i] == SONORUS_STUDIO))
	    {
	      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " in mode %d", audio_mode[i]);
	      pprint(audio_strbuf);
	      break;
	    }
	}
      pprint(")");
    }
#endif
  status = ioctl(fd, SOUND_MIXER_READ_RECSRC, &recsrc);
  if (status == -1) 
    {
      linux_audio_close(fd); 
      fd = -1;
      pprint(" no recsrc\n");
    }
  else
    {
      status = ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask);
      if ((status == -1) || (devmask == 0))
	{
	  linux_audio_close(fd); 
	  fd = -1;
	  if (status == -1) pprint(" no devmask\n"); else pprint(" (no reported devices)");
	}
      else
	{
	  status = ioctl(fd, SOUND_MIXER_READ_RECMASK, &recmask);
	  if (status == -1) 
	    {
	      pprint(" no recmask\n");
	      recmask = 0;
	    }
	  status = ioctl(fd, SOUND_MIXER_READ_STEREODEVS, &stereodevs);
	  if (status == -1) 
	    {
	      pprint(" no stereodevs\n");
	      stereodevs = 0;
	    }
	  status = ioctl(fd, SOUND_MIXER_READ_CAPS, &caps);
	  if (status == -1) 
	    {
	      pprint(" no caps\n");
	      caps = 0;
	    }
	  pprint(":\n\n"
		 "  mixer     recording  active     stereo    current\n"
		 "  channel   source     source     device    level\n"
		 "  --------  --------   --------   --------  -------- \n"); 
	  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	    {
	      if ((1<<i) & devmask)
		{
		  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %-10s", sound_device_names[i]); 
		  pprint(audio_strbuf);
		  yes_no((1 << i) & recmask);
		  yes_no((1 << i) & recsrc);
		  yes_no((1 << i) & stereodevs);
		  status = ioctl(fd, MIXER_READ(i), &level);
		  if (status != -1)
		    {
		      if ((1<<i) & stereodevs)
			mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %.2f %.2f", (float)(level & 0xff) * 0.01, (float)((level & 0xff00) >> 8) * 0.01);
		      else mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %.2f", (float)(level & 0xff) * 0.01);
		      /* can't use %% here because subsequent fprintf in pprint evaluates the %! #$@$! */
		      pprint(audio_strbuf);
		    }
		  pprint("\n"); 
		}
	    }
	  pprint("--------------------------------\n");
	}
    }

AUDIO_INFO:
  if (fd != -1) {linux_audio_close(fd); fd = -1;}
  mus_snprintf(dsp_name, LABEL_BUFFER_SIZE, "%s%d", DAC_NAME, dsp_num);
  fd = linux_audio_open(dsp_name, O_RDWR, 0, 0);
  if ((fd == -1) && (dsp_num == 0)) fd = linux_audio_open(DAC_NAME, O_WRONLY, 0, 0);
  if (fd == -1) return;
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s:\n\n", dsp_name); pprint(audio_strbuf);
  if ((ioctl(fd, SOUND_PCM_READ_RATE, &rate) != -1) &&
      (ioctl(fd, SOUND_PCM_READ_CHANNELS, &channels) != -1) &&
      (ioctl(fd, SOUND_PCM_READ_BITS, &bits) != -1) &&
      (ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &blocksize) != -1))
    {
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
		   "  defaults:\n    sampling rate: %d, chans: %d, sample size: %d bits, block size: %d bytes", 
		   rate, channels, bits, blocksize); 
      pprint(audio_strbuf);

#ifdef SNDCTL_DSP_GETOSPACE
      {
	audio_buf_info abi;
	ioctl(fd, SNDCTL_DSP_GETOSPACE, &abi);
	mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " (%d fragments)\n", abi.fragments);
	pprint(audio_strbuf);
      }
#else
      pprint("\n");
#endif
 
      deffmt = AFMT_QUERY;
      if ((ioctl(fd, SOUND_PCM_SETFMT, &deffmt) != -1) &&
	  (ioctl(fd, SOUND_PCM_GETFMTS, &formats) != -1))
	{
	  pprint("  supported formats:\n"); 
	  if (formats & AFMT_MU_LAW)    {pprint("    mulaw");                        if (deffmt == AFMT_MU_LAW)    pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_A_LAW)     {pprint("    alaw");                         if (deffmt == AFMT_A_LAW)     pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_IMA_ADPCM) {pprint("    adpcm");                        if (deffmt == AFMT_IMA_ADPCM) pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_U8)        {pprint("    unsigned byte");                if (deffmt == AFMT_U8)        pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_S16_LE)    {pprint("    signed little-endian short");   if (deffmt == AFMT_S16_LE)    pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_S16_BE)    {pprint("    signed big-endian short");      if (deffmt == AFMT_S16_BE)    pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_S8)        {pprint("    signed byte");                  if (deffmt == AFMT_S8)        pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_U16_LE)    {pprint("    unsigned little-endian short"); if (deffmt == AFMT_U16_LE)    pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_U16_BE)    {pprint("    unsigned big-endian short");    if (deffmt == AFMT_U16_BE)    pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_MPEG)      {pprint("    mpeg 2");                       if (deffmt == AFMT_MPEG)      pprint(" (default)"); pprint("\n");}
#ifdef NEW_OSS
	  if (formats & AFMT_S32_LE)    {pprint("    signed little-endian int");     if (deffmt == AFMT_S32_LE)    pprint(" (default)"); pprint("\n");}
	  if (formats & AFMT_S32_BE)    {pprint("    signed big-endian int");        if (deffmt == AFMT_S32_BE)    pprint(" (default)"); pprint("\n");}
#endif
	  status = ioctl(fd, SNDCTL_DSP_GETCAPS, &caps);
	  if (status != -1)
	    {
	      if (caps & DSP_CAP_DUPLEX) pprint("  full duplex\n");
	      pprint("            sample        srate\n  channels   size      min      max\n");
	      for (channels = 1; channels <= 2; channels++)
		{
		  for (bits = 8; bits <= 16; bits += 8)
		    {
		      min_rate = 1;
		      if (set_dsp(fd, channels, bits, &min_rate) == -1) continue;
		      max_rate = 100000;
		      if (set_dsp(fd, channels, bits, &max_rate) == -1) continue;
		      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %4d  %8d  %8d  %8d\n", channels, bits, min_rate, max_rate); 
		      pprint(audio_strbuf);
		    }
		}
	    }
	}
    }
  pprint("--------------------------------\n");
  linux_audio_close(fd); 
  fd = -1;
  dsp_num++; 
  if (dsp_num < 16)
    {
      mus_snprintf(dsp_name, LABEL_BUFFER_SIZE, "%s%d", MIXER_NAME, dsp_num);
      goto MIXER_INFO;
    }
}

/* ------------------------------- ALSA, OSS, Jack ----------------------------------- */
/* API being used */

static int api = ALSA_API;
int mus_audio_api(void) {return(api);}

/* hopefully first call to sndlib will be this... */
static int probe_api(void);
static int (*vect_mus_audio_initialize)(void);

/* FIXME: add a suitable default for all other vectors
   so that a call happening before mus_audio_initialize
   can be detected */
/* I don't think this is necessary -- documentation discusses this
 * (mus_sound_initialize calls mus_audio_initialize)
 */

/* vectors for the rest of the sndlib api */
static void  (*vect_mus_oss_set_buffers)(int num, int size);
static int   (*vect_mus_audio_systems)(void);
static char* (*vect_mus_audio_system_name)(int system);
static char* (*vect_mus_audio_moniker)(void);
static int   (*vect_mus_audio_open_output)(int ur_dev, int srate, int chans, int format, int size);
static int   (*vect_mus_audio_open_input)(int ur_dev, int srate, int chans, int format, int requested_size);
static int   (*vect_mus_audio_write)(int id, char *buf, int bytes);
static int   (*vect_mus_audio_read)(int id, char *buf, int bytes);
static int   (*vect_mus_audio_close)(int id);
static int   (*vect_mus_audio_mixer_read)(int ur_dev, int field, int chan, float *val);
static int   (*vect_mus_audio_mixer_write)(int ur_dev, int field, int chan, float *val);
static void  (*vect_describe_audio_state_1)(void);

/* vectors for the rest of the sndlib api */
int mus_audio_initialize(void) 
{
  return(probe_api());
}

void mus_oss_set_buffers(int num, int size) 
{
  vect_mus_oss_set_buffers(num, size);
}

int mus_audio_systems(void) 
{
  return(vect_mus_audio_systems());
}

char* mus_audio_system_name(int system) 
{
  return(vect_mus_audio_system_name(system));
}

#if HAVE_ALSA 
static char* alsa_mus_audio_moniker(void);
#endif

char* mus_audio_moniker(void) 
{
#if (HAVE_OSS && HAVE_ALSA)
  char *both_names;
  both_names = (char *)CALLOC(PRINT_BUFFER_SIZE, sizeof(char));
  /* need to be careful here since these use the same constant buffer */
  strcpy(both_names, oss_mus_audio_moniker());
  strcat(both_names, ", ");
  strcat(both_names, alsa_mus_audio_moniker());
  return(both_names); /* tiny memory leak ... */
#else
  return(vect_mus_audio_moniker());
#endif
}

int mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size) 
{
  return(vect_mus_audio_open_output(ur_dev, srate, chans, format, size));
}

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int requested_size) 
{
  return(vect_mus_audio_open_input(ur_dev, srate, chans, format, requested_size));
}

int mus_audio_write(int id, char *buf, int bytes) 
{
  return(vect_mus_audio_write(id, buf, bytes));
}

int mus_audio_read(int id, char *buf, int bytes) 
{
  return(vect_mus_audio_read(id, buf, bytes));
}

int mus_audio_close(int id) 
{
  return(vect_mus_audio_close(id));
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val) 
{
  return(vect_mus_audio_mixer_read(ur_dev, field, chan, val));
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val) 
{
  return(vect_mus_audio_mixer_write(ur_dev, field, chan, val));
}

static void describe_audio_state_1(void) 
{
  vect_describe_audio_state_1();
}

#if HAVE_JACK
  static int jack_mus_audio_initialize(void);
#endif

#if (!HAVE_ALSA)
static int probe_api(void) 
{
#if HAVE_JACK
  {
    int jackprobe = jack_mus_audio_initialize();
    if (jackprobe == MUS_ERROR)
      {
#endif
  /* go for the oss api */
  api = OSS_API;
  vect_mus_audio_initialize = oss_mus_audio_initialize;
  vect_mus_oss_set_buffers = oss_mus_oss_set_buffers;
  vect_mus_audio_systems = oss_mus_audio_systems;
  vect_mus_audio_system_name = oss_mus_audio_system_name;
  vect_mus_audio_moniker = oss_mus_audio_moniker;
  vect_mus_audio_open_output = oss_mus_audio_open_output;
  vect_mus_audio_open_input = oss_mus_audio_open_input;
  vect_mus_audio_write = oss_mus_audio_write;
  vect_mus_audio_read = oss_mus_audio_read;
  vect_mus_audio_close = oss_mus_audio_close;
  vect_mus_audio_mixer_read = oss_mus_audio_mixer_read;
  vect_mus_audio_mixer_write = oss_mus_audio_mixer_write;
  vect_describe_audio_state_1 = oss_describe_audio_state_1;
  return(vect_mus_audio_initialize());
#if HAVE_JACK
      }
    return(jackprobe);
  }
#endif
}
#endif

#endif


/* ------------------------------- ALSA ----------------------------------------- */
/*
 * added HAVE_NEW_ALSA, and changed various calls to reflect the new calling sequences (all under HAVE_NEW_ALSA)
 *   also scheme/ruby tie-ins, and other such changes.  Changed the names of the environment variables to use MUS, not SNDLIB.
 * reformatted and reorganized to be like the rest of the code
 * changed default device to "default"
 *    -- Bill 3-Feb-06
 *
 * error handling (mus_error) changed by Bill 14-Nov-02
 * 0.5 support removed by Bill 24-Mar-02
 *
 * changed for 0.9.x api by Fernando Lopez-Lezcano <nando@ccrma.stanford.edu>
 *
 *  sndlib "exports" only one soundcard with two directions (if they are available),
 *  and only deals with the alsa library pcm's. It does not scan for available
 *  cards and devices at the hardware level. Which device it uses can be defined by:
 *
 *  - setting variables in the environment (searched for in the following order):
 *    MUS_ALSA_PLAYBACK_DEVICE
 *       defines the name of the playback device
 *    MUS_ALSA_CAPTURE_DEVICE
 *       defines the name of the capture device
 *    MUS_ALSA_DEVICE
 *       defines the name of the playback and capture device
 *    use the first two if the playback and capture devices are different or the
 *    third if they are the same. 
 *  - if no variables are found in the environment sndlib tries to probe for a
 *    default device named "sndlib" (in alsa 0.9 devices are configured in 
 *    /usr/share/alsa/alsa.conf or in ~/.asoundrc)
 *  - if "sndlib" is not a valid device "hw:0,0" was used [but now it looks for "default"] (which by default should
 *    point to the first device of the first card
 *
 *  Some default settings are controllable through the environment as well:
 *    MUS_ALSA_BUFFER_SIZE = size of each buffer in frames
 *    MUS_ALSA_BUFFERS = number of buffers
 *
 * changed 18-Sep-00 by Bill: new error handling: old mus_audio_error folded into
 *  mus_error; mus_error itself should be used only for "real" errors -- things
 *  that can cause a throw (a kind of global jump elsewhere); use mus_print for informational
 *  stuff -- in Snd, mus_print will also save everything printed in the error dialog.
 *  In a few cases, I tried to fix the code to unwind before mus_error, and in others
 *  I've changed mus_error to mus_print, but some of these may be mistaken.
 *  Look for ?? below for areas where I'm not sure I rewrote code correctly.
 *
 * changed for 0.6.x api by Paul Barton-Davis, pbd@op.net
 *
 * changed for 0.5.x api by Fernando Lopez-Lezcano, nando@ccrma.stanford.edu
 *   04-10-2000:
 *     based on original 0.4.x code by Paul Barton-Davis (not much left of it :-)
 *     also Bill's code and Jaroslav Kysela (aplay.c and friends)
 *
 * Changes:
 * 04/25/2000: finished major rework, snd-dac now automatically decides which
 *             device or devices it uses for playback. Multiple device use is
 *             for now restricted to only two at most (more changes in Bill's
 *             needed to be able to support more). Four channel playback in 
 *             Ensoniq AudioPCI and relatives possible (with proper settings
 *             of the mixer) as well as using two separate cards. 
 * 04/11/2000: added reporting of alsa sound formats
*/

#if HAVE_ALSA

#if (!HAVE_OSS)
#define AUDIO_OK
#endif

#include <sys/ioctl.h>

#if (!HAVE_NEW_ALSA)
  #define ALSA_PCM_OLD_HW_PARAMS_API
  #define ALSA_PCM_OLD_SW_PARAMS_API
#endif

#if HAVE_ALSA_ASOUNDLIB_H
  #include <alsa/asoundlib.h>
#else
  #include <sys/asoundlib.h>
#endif

#if SND_LIB_VERSION < ((0<<16)|(6<<8)|(0))
  #error ALSA version is too old -- audio.c needs 0.9 or later
#endif

/* prototypes for the alsa sndlib functions */
static int   alsa_mus_audio_initialize(void);
static void  alsa_mus_oss_set_buffers(int num, int size);
static int   alsa_mus_audio_systems(void);
static char* alsa_mus_audio_system_name(int system);
static int   alsa_mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size);
static int   alsa_mus_audio_open_input(int ur_dev, int srate, int chans, int format, int requested_size);
static int   alsa_mus_audio_write(int id, char *buf, int bytes);
static int   alsa_mus_audio_read(int id, char *buf, int bytes);
static int   alsa_mus_audio_close(int id);
static int   alsa_mus_audio_mixer_read(int ur_dev, int field, int chan, float *val);
static int   alsa_mus_audio_mixer_write(int ur_dev, int field, int chan, float *val);
static void  alsa_describe_audio_state_1(void);

/* decide which api to activate */

static int probe_api(void) 
{
#if HAVE_JACK
  int jackprobe;
  jackprobe = jack_mus_audio_initialize();
  if (jackprobe == MUS_ERROR)
    {
#endif
    int card = -1;
    if ((snd_card_next(&card) >= 0) && (card >= 0))
      {
	/* the alsa library has detected one or more cards */
	api = ALSA_API;
	vect_mus_audio_initialize = alsa_mus_audio_initialize;
	vect_mus_oss_set_buffers = alsa_mus_oss_set_buffers;
	vect_mus_audio_systems = alsa_mus_audio_systems;
	vect_mus_audio_system_name = alsa_mus_audio_system_name;
	vect_mus_audio_moniker = alsa_mus_audio_moniker;
	vect_mus_audio_open_output = alsa_mus_audio_open_output;
	vect_mus_audio_open_input = alsa_mus_audio_open_input;
	vect_mus_audio_write = alsa_mus_audio_write;
	vect_mus_audio_read = alsa_mus_audio_read;
	vect_mus_audio_close = alsa_mus_audio_close;
	vect_mus_audio_mixer_read = alsa_mus_audio_mixer_read;
	vect_mus_audio_mixer_write = alsa_mus_audio_mixer_write;
	vect_describe_audio_state_1 = alsa_describe_audio_state_1;
      } 
    else 
      {
	/* go for the oss api */
        api = OSS_API;
	vect_mus_audio_initialize = oss_mus_audio_initialize;
	vect_mus_oss_set_buffers = oss_mus_oss_set_buffers;
	vect_mus_audio_systems = oss_mus_audio_systems;
	vect_mus_audio_system_name = oss_mus_audio_system_name;
	vect_mus_audio_moniker = oss_mus_audio_moniker;
	vect_mus_audio_open_output = oss_mus_audio_open_output;
	vect_mus_audio_open_input = oss_mus_audio_open_input;
	vect_mus_audio_write = oss_mus_audio_write;
	vect_mus_audio_read = oss_mus_audio_read;
	vect_mus_audio_close = oss_mus_audio_close;
	vect_mus_audio_mixer_read = oss_mus_audio_mixer_read;
	vect_mus_audio_mixer_write = oss_mus_audio_mixer_write;
	vect_describe_audio_state_1 = oss_describe_audio_state_1;
      }
    /* will the _real_ mus_audio_initialize please stand up? */
    return(vect_mus_audio_initialize());
#if HAVE_JACK
    }
  return(jackprobe);
#endif
}

/* convert a sndlib sample format to an alsa sample format */

static snd_pcm_format_t to_alsa_format(int snd_format)
{
  switch (snd_format) 
    {
    case MUS_BYTE:     return(SND_PCM_FORMAT_S8); 
    case MUS_UBYTE:    return(SND_PCM_FORMAT_U8); 
    case MUS_MULAW:    return(SND_PCM_FORMAT_MU_LAW); 
    case MUS_ALAW:     return(SND_PCM_FORMAT_A_LAW); 
    case MUS_BSHORT:   return(SND_PCM_FORMAT_S16_BE); 
    case MUS_LSHORT:   return(SND_PCM_FORMAT_S16_LE); 
    case MUS_UBSHORT:  return(SND_PCM_FORMAT_U16_BE); 
    case MUS_ULSHORT:  return(SND_PCM_FORMAT_U16_LE); 
    case MUS_B24INT:   return(SND_PCM_FORMAT_S24_BE); 
    case MUS_L24INT:   return(SND_PCM_FORMAT_S24_LE); 
    case MUS_BINT:     return(SND_PCM_FORMAT_S32_BE); 
    case MUS_LINT:     return(SND_PCM_FORMAT_S32_LE); 
    case MUS_BINTN:    return(SND_PCM_FORMAT_S32_BE); 
    case MUS_LINTN:    return(SND_PCM_FORMAT_S32_LE); 
    case MUS_BFLOAT:   return(SND_PCM_FORMAT_FLOAT_BE); 
    case MUS_LFLOAT:   return(SND_PCM_FORMAT_FLOAT_LE); 
    case MUS_BDOUBLE:  return(SND_PCM_FORMAT_FLOAT64_BE); 
    case MUS_LDOUBLE:  return(SND_PCM_FORMAT_FLOAT64_LE); 
    }
  return((snd_pcm_format_t)MUS_ERROR);
}

/* FIXME: this is not taking yet into account the 
 * number of bits that a given alsa format is actually
 * using... 
 */

static int to_mus_format(int alsa_format) 
{
  /* alsa format definitions from asoundlib.h (0.9 cvs 6/27/2001) */
  switch (alsa_format)
    {
    case SND_PCM_FORMAT_S8:         return(MUS_BYTE);
    case SND_PCM_FORMAT_U8:         return(MUS_UBYTE);
    case SND_PCM_FORMAT_S16_LE:     return(MUS_LSHORT);
    case SND_PCM_FORMAT_S16_BE:     return(MUS_BSHORT);
    case SND_PCM_FORMAT_U16_LE:     return(MUS_ULSHORT);
    case SND_PCM_FORMAT_U16_BE:     return(MUS_UBSHORT);
    case SND_PCM_FORMAT_S24_LE:     return(MUS_L24INT);
    case SND_PCM_FORMAT_S24_BE:     return(MUS_B24INT);
    case SND_PCM_FORMAT_S32_LE:     return(MUS_LINTN); /* 32bit normalized plays 24bit and 16bit files with same amplitude bound (for 24 bit cards) */
    case SND_PCM_FORMAT_S32_BE:     return(MUS_BINTN);
    case SND_PCM_FORMAT_FLOAT_LE:   return(MUS_LFLOAT);
    case SND_PCM_FORMAT_FLOAT_BE:   return(MUS_BFLOAT);
    case SND_PCM_FORMAT_FLOAT64_LE: return(MUS_LDOUBLE);
    case SND_PCM_FORMAT_FLOAT64_BE: return(MUS_BDOUBLE);
    case SND_PCM_FORMAT_MU_LAW:     return(MUS_MULAW);
    case SND_PCM_FORMAT_A_LAW:      return(MUS_ALAW);
    /* formats with no translation in snd */
    case SND_PCM_FORMAT_U24_LE:
    case SND_PCM_FORMAT_U24_BE:
    case SND_PCM_FORMAT_U32_LE:
    case SND_PCM_FORMAT_U32_BE:
    case SND_PCM_FORMAT_IEC958_SUBFRAME_LE:
    case SND_PCM_FORMAT_IEC958_SUBFRAME_BE:
    case SND_PCM_FORMAT_IMA_ADPCM:
    case SND_PCM_FORMAT_MPEG:
    case SND_PCM_FORMAT_GSM:
    case SND_PCM_FORMAT_SPECIAL:
    default:
      return(MUS_ERROR);
    }
}

/* convert a sndlib device into an alsa device number and channel
 * [has to be coordinated with following function!] 
 */

/* very simplistic approach, device mapping should also depend
 * on which card we're dealing with, digital i/o devices should
 * be identified as such and so on 
 */

/* NOTE: in the Delta1010 digital i/o is just a pair of channels
 * in the 10 channel playback frame or 12 channel capture frame,
 * how do we specify that???
 */

static int to_alsa_device(int dev, int *adev, snd_pcm_stream_t *achan)
{
  switch(dev) 
    {
      /* default values are a problem because the concept does
       * not imply a direction (playback or capture). This works
       * fine as long as both directions of a device are symetric,
       * the Midiman 1010, for example, has 10 channel frames for
       * playback and 12 channel frames for capture and breaks 
       * the recorder (probes the default, defaults to output, 
       * uses the values for input). 
       */
    case MUS_AUDIO_DEFAULT:
    case MUS_AUDIO_DUPLEX_DEFAULT:
    case MUS_AUDIO_LINE_OUT:
      /* analog output */
      (*adev) = 0;
      (*achan) = SND_PCM_STREAM_PLAYBACK;
      break;
    case MUS_AUDIO_AUX_OUTPUT:
      /* extra analog output */
      (*adev) = 1;
      (*achan) = SND_PCM_STREAM_PLAYBACK;
      break;
    case MUS_AUDIO_DAC_OUT:
      /* analog outputs */
      (*adev) = 2;
      (*achan) = SND_PCM_STREAM_PLAYBACK;
      break;
    case MUS_AUDIO_MICROPHONE:
    case MUS_AUDIO_LINE_IN:
      /* analog input */
      (*adev) = 0;
      (*achan) = SND_PCM_STREAM_CAPTURE;
      break;
    case MUS_AUDIO_AUX_INPUT:
      /* extra analog input */
      (*adev) = 1;
      (*achan) = SND_PCM_STREAM_CAPTURE;
      break;
    case MUS_AUDIO_DIGITAL_OUT:
    case MUS_AUDIO_SPDIF_OUT:
    case MUS_AUDIO_AES_OUT:
    case MUS_AUDIO_ADAT_OUT:
    case MUS_AUDIO_DIGITAL_IN:
    case MUS_AUDIO_SPDIF_IN:
    case MUS_AUDIO_AES_IN:
    case MUS_AUDIO_ADAT_IN:
    case MUS_AUDIO_SPEAKERS:
    case MUS_AUDIO_DAC_FILTER:
    case MUS_AUDIO_MIXER:
    case MUS_AUDIO_LINE1:
    case MUS_AUDIO_LINE2:
    case MUS_AUDIO_LINE3:
    case MUS_AUDIO_CD:
    default:
      return(MUS_ERROR);
      break;
    }
  return(0);
}

/* convert an alsa device into a sndlib device 
 * [has to be coordinated with previous function!] 
 *
 * naming here is pretty much arbitrary. We have to have
 * a bidirectional mapping between sndlib devices and
 * alsa devices and that's just not possible (I think). 
 * This stopgap mapping ignores digital input and output
 * devices - how to differentiate them in alsa?
 */

static int to_sndlib_device(int dev, int channel) 
{
  switch (channel) 
    {
    case SND_PCM_STREAM_PLAYBACK:
      switch (dev) 
	{
	  /* works only for the first three outputs */
	case 0: return(MUS_AUDIO_LINE_OUT);
	case 1: return(MUS_AUDIO_AUX_OUTPUT);
	case 2: return(MUS_AUDIO_DAC_OUT);
	default:
	  return(MUS_ERROR);
	}
    case SND_PCM_STREAM_CAPTURE:
      switch (dev) 
	{
	case 0: return(MUS_AUDIO_LINE_IN);
	case 1: return(MUS_AUDIO_AUX_INPUT);
	default:
	  return(MUS_ERROR);
	}
      break;
    }
  return(MUS_ERROR);
}


static int alsa_mus_error(int type, char *message)
{
  if (message)
    {
      mus_print(message);
      FREE(message);
    }
  return(MUS_ERROR);
}

#if 0
static void alsa_dump_hardware_params(snd_pcm_hw_params_t *params, const char *msg) 
{
  snd_output_t *out;
  snd_output_stdio_attach(&out, stderr, 0);
  fprintf(stderr, "%s\n", msg);
  snd_pcm_hw_params_dump(params, out);
}

static void alsa_dump_software_params(snd_pcm_sw_params_t *params, const char *msg) 
{
  snd_output_t *out;
  snd_output_stdio_attach(&out, stderr, 0);
  fprintf(stderr, "%s\n", msg);
  snd_pcm_sw_params_dump(params, out);
}
#endif


/* dump current hardware and software configuration */

static void alsa_dump_configuration(char *name, snd_pcm_hw_params_t *hw_params, snd_pcm_sw_params_t *sw_params)
{
  int err; 
  char *str;
  size_t len;
  snd_output_t *buf;

#if (SND_LIB_MAJOR == 0) || ((SND_LIB_MAJOR == 1) && (SND_LIB_MINOR == 0) && (SND_LIB_SUBMINOR < 8))
  return; /* avoid Alsa bug */
#endif

  err = snd_output_buffer_open(&buf);
  if (err < 0) 
    {
      mus_print("could not open dump buffer: %s", snd_strerror(err));
    } 
  else 
    {
      if (hw_params) 
	{
	  snd_output_puts(buf, "hw_params status of ");
	  snd_output_puts(buf, name);
	  snd_output_puts(buf, "\n");
	  err = snd_pcm_hw_params_dump(hw_params, buf);
	  if (err < 0) 
	    mus_print("snd_pcm_hw_params_dump: %s", snd_strerror(err));
	}
      if (sw_params) 
	{
	  snd_output_puts(buf, "sw_params status of ");
	  snd_output_puts(buf, name);
	  snd_output_puts(buf, "\n");
	  err = snd_pcm_sw_params_dump(sw_params, buf);
	  if (err < 0) 
	    mus_print("snd_pcm_hw_params_dump: %s", snd_strerror(err));
	}
      snd_output_putc(buf, '\0');
      len = snd_output_buffer_string(buf, &str);
      if (len > 1) 
	mus_print("status of %s\n%s", name, str);
      snd_output_close(buf);
    }
}

/* get hardware params for a pcm */

static snd_pcm_hw_params_t *alsa_get_hardware_params(const char *name, snd_pcm_stream_t stream, int mode)
{
  int err;
  snd_pcm_t *handle;
  if ((err = snd_pcm_open(&handle, name, stream, mode | SND_PCM_NONBLOCK)) != 0) 
    {
      alsa_mus_error(MUS_AUDIO_CANT_OPEN, 
		     mus_format("open pcm %s for stream %d: %s",
				name, stream, snd_strerror(err)));
      return(NULL);
    }
  else 
    {
      snd_pcm_hw_params_t *params;
      params = (snd_pcm_hw_params_t *)calloc(1, snd_pcm_hw_params_sizeof());
      if (params == NULL) 
	{
	  snd_pcm_close(handle);
	  alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			 mus_format("could not allocate memory for hardware params"));
	} 
      else 
	{
	  err = snd_pcm_hw_params_any(handle, params);
	  if (err < 0) 
	    {
	      snd_pcm_close(handle);
	      alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			     mus_format("snd_pcm_hw_params_any: pcm %s, stream %d, error: %s",
					name, stream, snd_strerror(err)));
	    } 
	  else 
	    {
	      snd_pcm_close(handle);
	      return(params);
	    }
	}
    }
  return(NULL);
}

/* allocate software params structure */

static snd_pcm_sw_params_t *alsa_get_software_params(void)
{
  snd_pcm_sw_params_t *params = NULL;
  params = (snd_pcm_sw_params_t *)calloc(1, snd_pcm_sw_params_sizeof());
  if (params == NULL) 
    {
      alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
		     mus_format("could not allocate memory for software params"));
    } 
  return(params);
}

/* probe a device name against the list of available pcm devices */

#ifndef SND_CONFIG_GET_ID_ARGS
  #define SND_CONFIG_GET_ID_ARGS 1
#endif

static bool alsa_probe_device_name(const char *name)
{
  snd_config_t *conf;
  snd_config_iterator_t pos, next;
  int err;
  
  err = snd_config_update();
  if (err < 0) 
    {
      mus_print("snd_config_update: %s", snd_strerror(err));
      return(false);
    }

  err = snd_config_search(snd_config, "pcm", &conf);
  if (err < 0) 
    {
      mus_print("snd_config_search: %s", snd_strerror(err));
      return(false);
    }

  snd_config_for_each(pos, next, conf) 
    {
      snd_config_t *c = snd_config_iterator_entry(pos);
#if (SND_CONFIG_GET_ID_ARGS == 2)
      const char *id;
      int err = snd_config_get_id(c, &id);
      if (err == 0) {
	int result = strncmp(name, id, strlen(id));
	if (result == 0 &&
	    (name[strlen(id)] == '\0' || name[strlen(id)] == ':')) 
	  {
	    return(true);
	  }
      }
#else
      const char *id = snd_config_get_id(c);
      int result = strncmp(name, id, strlen(id));
      if (result == 0 &&
	  (name[strlen(id)] == '\0' || name[strlen(id)] == ':')) 
	{
	  return(true);
	}
#endif
    }
  return(false);
}

/* check a device name against the list of available pcm devices */

static int alsa_check_device_name(const char *name)
{
  if (!alsa_probe_device_name(name)) 
    {
      return(alsa_mus_error(MUS_AUDIO_CANT_READ, 
			    mus_format("alsa could not find device \"%s\" in configuration", 
				       name)));
    } 
  return(MUS_NO_ERROR);
}


/* set scheduling priority to SCHED_FIFO 
 * this will only work if the program that uses sndlib is run as root or is suid root 
 */

/* whether we want to trace calls 
 *
 * set to "1" to print function trace information in the 
 * snd error window
 */

static int alsa_trace = 0;

/* this should go away as it is oss specific */

static int fragment_size = 512; 
static int fragments = 4;

static void alsa_mus_oss_set_buffers(int num, int size) 
{
  fragments = num; 
  fragment_size = size; 
#if MUS_DEBUGGING
  mus_print("set_oss_buffers: %d fragments or size %d", num, size);
#endif
}

/* total number of soundcards in our setup, set by initialize_audio */

/* static int sound_cards = 0; */

/* return the number of cards that are available */

static int alsa_mus_audio_systems(void) 
{
  return(sound_cards);
}

/* return the type of driver we're dealing with */

static char *alsa_mus_audio_moniker(void)
{
  if (version_name == NULL) version_name = (char *)CALLOC(LABEL_BUFFER_SIZE, sizeof(char));
  mus_snprintf(version_name, LABEL_BUFFER_SIZE, "ALSA %s", SND_LIB_VERSION_STR);
  return(version_name);
}

/* handles for both directions of the virtual device */

static snd_pcm_t *handles[2] = {NULL, NULL};

/* hardware and software parameter sctructure pointers */

static snd_pcm_hw_params_t *alsa_hw_params[2] = {NULL, NULL}; /* avoid bogus free */
static snd_pcm_sw_params_t *alsa_sw_params[2] = {NULL, NULL};

/* some defaults */

static int alsa_open_mode = SND_PCM_ASYNC;
static int alsa_buffers = 3;
/* size of buffer in number of samples per channel, 
 * at 44100 approximately 5.9mSecs
 */
static int alsa_samples_per_channel = 1024;
static snd_pcm_access_t alsa_interleave = SND_PCM_ACCESS_RW_INTERLEAVED;
static int alsa_max_capture_channels = 32;

/* first default name for pcm configuration */

static char *alsa_sndlib_device_name = "sndlib";

/* second default for playback and capture: hardware pcm, first card, first device */
/* pcms used by sndlib, playback and capture */

static char *alsa_playback_device_name = NULL;
static char *alsa_capture_device_name = NULL;


/* -------- tie these names into scheme/ruby -------- */

static int alsa_get_max_buffers(void)
{
  unsigned int max_periods = 0, max_rec_periods = 0;
  int dir = 0;
#if HAVE_NEW_ALSA
  snd_pcm_hw_params_get_periods_max(alsa_hw_params[SND_PCM_STREAM_PLAYBACK], &max_periods, &dir);
#else
  max_periods = snd_pcm_hw_params_get_periods_max(alsa_hw_params[SND_PCM_STREAM_PLAYBACK], &dir);
#endif
  if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) 
    {
#if HAVE_NEW_ALSA
      snd_pcm_hw_params_get_periods_max(alsa_hw_params[SND_PCM_STREAM_CAPTURE], &max_rec_periods, &dir);
#else
      max_rec_periods = snd_pcm_hw_params_get_periods_max(alsa_hw_params[SND_PCM_STREAM_CAPTURE], &dir);
#endif
      if (max_periods > max_rec_periods) 
	max_periods = max_rec_periods;
    }
  return(max_periods);
}

static int alsa_get_min_buffers(void)
{
  unsigned int min_periods = 0, min_rec_periods = 0;
  int dir = 0;
#if HAVE_NEW_ALSA
  snd_pcm_hw_params_get_periods_min(alsa_hw_params[SND_PCM_STREAM_PLAYBACK], &min_periods, &dir);
#else
  min_periods = snd_pcm_hw_params_get_periods_min(alsa_hw_params[SND_PCM_STREAM_PLAYBACK], &dir);
#endif
  if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) 
    {
#if HAVE_NEW_ALSA
      snd_pcm_hw_params_get_periods_min(alsa_hw_params[SND_PCM_STREAM_CAPTURE], &min_rec_periods, &dir);
#else
      min_rec_periods = snd_pcm_hw_params_get_periods_min(alsa_hw_params[SND_PCM_STREAM_CAPTURE], &dir);
#endif
      if (min_periods < min_rec_periods) 
	min_periods = min_rec_periods;
    }
  return(min_periods);
}

static int alsa_clamp_buffers(int bufs)
{
  int minb, maxb;
  minb = alsa_get_min_buffers();
  maxb = alsa_get_max_buffers();
  if (bufs > maxb)
    bufs = maxb;
  if (bufs < minb)
    bufs = minb;
  return(bufs);
}

static snd_pcm_uframes_t alsa_get_min_buffer_size(void)
{
  snd_pcm_uframes_t min_buffer_size = 0, min_rec_buffer_size = 0;
#if HAVE_NEW_ALSA
  snd_pcm_hw_params_get_buffer_size_min(alsa_hw_params[SND_PCM_STREAM_PLAYBACK], &min_buffer_size);
#else
  min_buffer_size = snd_pcm_hw_params_get_buffer_size_min(alsa_hw_params[SND_PCM_STREAM_PLAYBACK]);
#endif
  if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) 
    {
#if HAVE_NEW_ALSA
      snd_pcm_hw_params_get_buffer_size_min(alsa_hw_params[SND_PCM_STREAM_CAPTURE], &min_rec_buffer_size);
#else
      min_rec_buffer_size = snd_pcm_hw_params_get_buffer_size_min(alsa_hw_params[SND_PCM_STREAM_CAPTURE]);
#endif

      if (min_buffer_size < min_rec_buffer_size) 
	min_buffer_size = min_rec_buffer_size;
    }
  return(min_buffer_size);
}

static snd_pcm_uframes_t alsa_get_max_buffer_size(void)
{
  snd_pcm_uframes_t max_buffer_size = 0, max_rec_buffer_size = 0;
#if HAVE_NEW_ALSA
  snd_pcm_hw_params_get_buffer_size_max(alsa_hw_params[SND_PCM_STREAM_PLAYBACK], &max_buffer_size);
#else
  max_buffer_size = snd_pcm_hw_params_get_buffer_size_max(alsa_hw_params[SND_PCM_STREAM_PLAYBACK]);
#endif
  if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) 
    {
#if HAVE_NEW_ALSA
      snd_pcm_hw_params_get_buffer_size_max(alsa_hw_params[SND_PCM_STREAM_CAPTURE], &max_rec_buffer_size);
#else
      max_rec_buffer_size = snd_pcm_hw_params_get_buffer_size_max(alsa_hw_params[SND_PCM_STREAM_CAPTURE]);
#endif
      if (max_buffer_size > max_rec_buffer_size) 
	max_buffer_size = max_rec_buffer_size;
    }
  return(max_buffer_size);
}

static int alsa_clamp_buffer_size(int buf_size)
{
  int minb, maxb;
  minb = alsa_get_min_buffer_size();
  maxb = alsa_get_max_buffer_size();
  if (buf_size > maxb)
    buf_size = maxb;
  if (buf_size < minb)
    buf_size = minb;
  return(buf_size);
}

static bool alsa_set_playback_parameters(void)
{
  /* playback stream parameters */
  if (alsa_hw_params[SND_PCM_STREAM_PLAYBACK]) free(alsa_hw_params[SND_PCM_STREAM_PLAYBACK]);
  alsa_hw_params[SND_PCM_STREAM_PLAYBACK] = alsa_get_hardware_params(alsa_playback_device_name, SND_PCM_STREAM_PLAYBACK, alsa_open_mode);
  if (alsa_hw_params[SND_PCM_STREAM_PLAYBACK]) 
    {
      snd_pcm_uframes_t size;
      int old_buffers;
      old_buffers = alsa_buffers;
      if (alsa_sw_params[SND_PCM_STREAM_PLAYBACK]) free(alsa_sw_params[SND_PCM_STREAM_PLAYBACK]);
      alsa_sw_params[SND_PCM_STREAM_PLAYBACK] = alsa_get_software_params();
      sound_cards = 1;
      alsa_buffers = alsa_clamp_buffers(alsa_buffers);
      if (alsa_buffers <= 0)
	{
	  alsa_buffers = old_buffers;
	  return(false);
	}
      size = alsa_clamp_buffer_size(alsa_samples_per_channel * alsa_buffers);
      if (size <= 0) return(false);
      alsa_samples_per_channel = size / alsa_buffers;
    }
  return(alsa_hw_params[SND_PCM_STREAM_PLAYBACK] && alsa_sw_params[SND_PCM_STREAM_PLAYBACK]);
}

static bool alsa_set_capture_parameters(void)
{  
  /* capture stream parameters */
  if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) free(alsa_hw_params[SND_PCM_STREAM_CAPTURE]);
  alsa_hw_params[SND_PCM_STREAM_CAPTURE] = alsa_get_hardware_params(alsa_capture_device_name, SND_PCM_STREAM_CAPTURE, alsa_open_mode);
  if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) 
    {
      snd_pcm_uframes_t size;
      int old_buffers;
      old_buffers = alsa_buffers;
      if (alsa_sw_params[SND_PCM_STREAM_CAPTURE]) free(alsa_sw_params[SND_PCM_STREAM_CAPTURE]);
      alsa_sw_params[SND_PCM_STREAM_CAPTURE] = alsa_get_software_params();
      sound_cards = 1;
      alsa_buffers = alsa_clamp_buffers(alsa_buffers);
      if (alsa_buffers <= 0)
	{
	  alsa_buffers = old_buffers;
	  return(false);
	}
      size = alsa_clamp_buffer_size(alsa_samples_per_channel * alsa_buffers);
      if (size <= 0) return(false);
      alsa_samples_per_channel = size / alsa_buffers;
    }
  return(alsa_hw_params[SND_PCM_STREAM_CAPTURE] && alsa_sw_params[SND_PCM_STREAM_CAPTURE]);
}


char *mus_alsa_playback_device(void) {return(alsa_playback_device_name);}
char *mus_alsa_set_playback_device(const char *name) 
{
  if (alsa_check_device_name(name) == MUS_NO_ERROR)
    {
      char *old_name = alsa_playback_device_name;
      alsa_playback_device_name = strdup(name); 
      if (!alsa_set_playback_parameters())
	{
	  alsa_playback_device_name = old_name; /* try to back out of the mistake */
	  alsa_set_playback_parameters();
	}
    }
  return(alsa_playback_device_name);
}

char *mus_alsa_capture_device(void) {return(alsa_capture_device_name);}
char *mus_alsa_set_capture_device(const char *name) 
{
  if (alsa_check_device_name(name) == MUS_NO_ERROR)
    {
      char *old_name = alsa_capture_device_name;
      alsa_capture_device_name = strdup(name); 
      if (!alsa_set_capture_parameters())
	{
	  alsa_capture_device_name = old_name;
	  alsa_set_capture_parameters();
	}
    }
  return(alsa_capture_device_name);
}

char *mus_alsa_device(void) {return(alsa_sndlib_device_name);}
char *mus_alsa_set_device(const char *name) 
{
  if (alsa_check_device_name(name) == MUS_NO_ERROR)
    {
      alsa_sndlib_device_name = strdup(name);
      mus_alsa_set_playback_device(name);
      mus_alsa_set_capture_device(name);
    }
  return(alsa_sndlib_device_name);
}

int mus_alsa_buffer_size(void) {return(alsa_samples_per_channel);}
int mus_alsa_set_buffer_size(int size) 
{
  snd_pcm_uframes_t bsize;
  if (alsa_buffers == 0) alsa_buffers = 1;
  if (size > 0)
    {
      bsize = alsa_clamp_buffer_size(size * alsa_buffers);
      alsa_samples_per_channel = bsize / alsa_buffers;
    }
  return(alsa_samples_per_channel);
}

int mus_alsa_buffers(void) {return(alsa_buffers);}
int mus_alsa_set_buffers(int num) 
{
  snd_pcm_uframes_t size;
  if (num > 0)
    {
      alsa_buffers = alsa_clamp_buffers(num);
      if (alsa_buffers > 0)
	{
	  size = alsa_clamp_buffer_size(alsa_samples_per_channel * alsa_buffers);
	  alsa_samples_per_channel = size / alsa_buffers;
	}
    }
  return(alsa_buffers);
}

static bool alsa_squelch_warning = false;
bool mus_alsa_squelch_warning(void) {return(alsa_squelch_warning);}
bool mus_alsa_set_squelch_warning(bool val) 
{
  alsa_squelch_warning = val; 
  return(val);
}




/* return the name of a given system */

static char *alsa_mus_audio_system_name(int system) 
{
  return(alsa_playback_device_name);
}

/* get a device name from the environment */

static char *alsa_get_device_from_env(const char *name)
{
  char *string = getenv(name);
  if (string) 
    if (alsa_check_device_name(string) == MUS_NO_ERROR) 
      return(string);
  return(NULL);
}

/* get an integer from the environment */

static int alsa_get_int_from_env(const char *name, int *value, int min, int max)
{
  char *string = getenv(name);
  if (string) 
    {
      char *end;
      long int result = strtol(string, &end, 10);
      if (((min != -1) && (max != -1)) &&
	  (result < min || result > max)) 
	{
	  return(alsa_mus_error(MUS_AUDIO_CANT_READ, 
				mus_format("%s ignored: out of range, value=%d, min=%d, max=%d",
					   name, (int)result, min, max)));
	} 
      else 
	{
	  if (errno == ERANGE) 
	    {
	      return(alsa_mus_error(MUS_AUDIO_CANT_READ, 
				    mus_format("%s ignored: strlol conversion out of range",
					       name)));
	    } 
	  else 
	    {
	      if ((*string != '\0') && (*end == '\0'))
		{
		  *value = (int)result;
		  return(MUS_NO_ERROR);
		} 
	      else 
		{
		  return(alsa_mus_error(MUS_AUDIO_CANT_READ, 
					mus_format("%s ignored: value is \"%s\", not an integer",
						   name, string)));
		}
	    }
	}
    }
  return(MUS_ERROR);
}

/* initialize the audio subsystem */

/* define environment variable names */
#define MUS_ALSA_PLAYBACK_DEVICE_ENV_NAME "MUS_ALSA_PLAYBACK_DEVICE"
#define MUS_ALSA_CAPTURE_DEVICE_ENV_NAME  "MUS_ALSA_CAPTURE_DEVICE"
#define MUS_ALSA_DEVICE_ENV_NAME          "MUS_ALSA_DEVICE"
#define MUS_ALSA_BUFFERS_ENV_NAME         "MUS_ALSA_BUFFERS"
#define MUS_ALSA_BUFFER_SIZE_ENV_NAME     "MUS_ALSA_BUFFER_SIZE"
#define MUS_ALSA_TRACE_ENV_NAME           "MUS_ALSA_TRACE"

static int alsa_mus_audio_initialize(void) 
{
  char *name = NULL;
  char *pname;
  char *cname;
  int value = 0, alsa_buffer_size = 0;

  if (audio_initialized) 
    return(0);

  sound_cards = 0;

  /* get trace flag from environment */
  if (alsa_get_int_from_env(MUS_ALSA_TRACE_ENV_NAME, &value, 0, 1) == MUS_NO_ERROR) 
    alsa_trace = value;

  /* try to get device names from environment */
  pname = alsa_get_device_from_env(MUS_ALSA_PLAYBACK_DEVICE_ENV_NAME);
  if ((pname) && (alsa_probe_device_name(pname)))
    alsa_playback_device_name = pname;

  cname = alsa_get_device_from_env(MUS_ALSA_CAPTURE_DEVICE_ENV_NAME);
  if ((cname) && (alsa_probe_device_name(cname)))
    alsa_capture_device_name = cname;
    
  name = alsa_get_device_from_env(MUS_ALSA_DEVICE_ENV_NAME);
  if ((name) && (alsa_probe_device_name(name)))
    {
      if (!alsa_playback_device_name) 
	alsa_playback_device_name = name;

      if (!alsa_capture_device_name) 
	alsa_capture_device_name = name;

      alsa_sndlib_device_name = name;
    }

  /* now check that we have a plausible name */
  if (!alsa_probe_device_name(alsa_sndlib_device_name))
    {
      alsa_sndlib_device_name = "default";
      if (!alsa_probe_device_name(alsa_sndlib_device_name))
	{
	  alsa_sndlib_device_name = "plughw:0";
	  if (!alsa_probe_device_name(alsa_sndlib_device_name))
	    alsa_sndlib_device_name = "hw:0";
	}
    }
    
  /* if no device name set yet, try for special sndlib name first */
  if (!alsa_playback_device_name) 
    {
      if (alsa_probe_device_name(alsa_sndlib_device_name)) 
	alsa_playback_device_name = alsa_sndlib_device_name;
      else alsa_playback_device_name = "hw:0";
    }

  if (!alsa_capture_device_name) 
    {
      if (alsa_probe_device_name(alsa_sndlib_device_name)) 
	alsa_capture_device_name = alsa_sndlib_device_name;
      else alsa_capture_device_name = "hw:0";
    }

  alsa_get_int_from_env(MUS_ALSA_BUFFERS_ENV_NAME, &alsa_buffers, -1, -1);
  alsa_get_int_from_env(MUS_ALSA_BUFFER_SIZE_ENV_NAME, &alsa_buffer_size, -1, -1);

  if ((alsa_buffer_size > 0) && (alsa_buffers > 0))
    alsa_samples_per_channel = alsa_buffer_size / alsa_buffers;

  if (!alsa_set_playback_parameters())
    {
      /* somehow we got a device that passed muster with alsa_probe_device_name, but doesn't return hw params! */
      alsa_playback_device_name = "plughw:0";
      if (!alsa_set_playback_parameters())
	{
	  alsa_playback_device_name = "hw:0";
	  if (!alsa_set_playback_parameters())
	    return(MUS_ERROR);
	}
    }

  if (!alsa_set_capture_parameters())
    {
      alsa_capture_device_name = "plughw:0";
      if (!alsa_set_capture_parameters())
	{
	  alsa_capture_device_name = "hw:0";
	  if (!alsa_set_capture_parameters())
	    return(MUS_ERROR);
	}
    }

  if ((!alsa_hw_params[SND_PCM_STREAM_CAPTURE]) ||
      (!alsa_hw_params[SND_PCM_STREAM_PLAYBACK]))
    return(MUS_ERROR);

  audio_initialized = true;
  return(0);
}

/* open an input or output stream */

static int alsa_audio_open(int ur_dev, int srate, int chans, int format, int size)
{
  int card, device, alsa_device;
  snd_pcm_format_t alsa_format;
  snd_pcm_stream_t alsa_stream;
  char *alsa_name;
  int frames, periods;
  int err;
  unsigned int r;
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *hw_params = NULL;
  snd_pcm_sw_params_t *sw_params = NULL;
  
  if ((!audio_initialized) && 
      (mus_audio_initialize() != MUS_NO_ERROR))
    return(MUS_ERROR);
  if (chans <= 0) return(MUS_ERROR);
  
  if (alsa_trace) 
    mus_print("%s: %x rate=%d, chans=%d, format=%d:%s, size=%d", 
	      c__FUNCTION__, ur_dev, srate, chans, format, 
	      mus_audio_format_name(format), size);

  card = MUS_AUDIO_SYSTEM(ur_dev);
  device = MUS_AUDIO_DEVICE(ur_dev);

  if ((err = to_alsa_device(device, &alsa_device, &alsa_stream)) < 0) 
    {
      return(alsa_mus_error(MUS_AUDIO_DEVICE_NOT_AVAILABLE, 
			    mus_format("%s: cannot translate device %s<%d> to alsa",
				       snd_strerror(err), mus_audio_device_name(device), device)));
    }
  if ((alsa_format = to_alsa_format(format)) == (snd_pcm_format_t)MUS_ERROR) 
    {
      return(alsa_mus_error(MUS_AUDIO_FORMAT_NOT_AVAILABLE, 
			    mus_format("could not change %s<%d> to alsa format", 
				       mus_audio_format_name(format), format)));
    }

  alsa_name = (alsa_stream == SND_PCM_STREAM_PLAYBACK) ? alsa_playback_device_name : alsa_capture_device_name;
  if ((err = snd_pcm_open(&handle, alsa_name, alsa_stream, alsa_open_mode)) != 0) 
    {
      snd_pcm_close(handle);
      return(alsa_mus_error(MUS_AUDIO_CANT_OPEN, 
			    mus_format("open pcm %s (%s) stream %s: %s",
				       mus_audio_device_name(device), alsa_name, snd_pcm_stream_name(alsa_stream), 
				       snd_strerror(err))));
    }
  handles[alsa_stream] = handle;
  hw_params = alsa_hw_params[alsa_stream];
  sw_params = alsa_sw_params[alsa_stream];
  if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: no parameter configurations available for %s", 
				       snd_strerror(err), alsa_name)));
    }

  err = snd_pcm_hw_params_set_access(handle, hw_params, alsa_interleave);
  if (err < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: %s: access type %s not available", 
				       snd_strerror(err), alsa_name, snd_pcm_access_name(alsa_interleave))));
    }

  periods = alsa_buffers;
  err = snd_pcm_hw_params_set_periods(handle, hw_params, periods, 0);
  if (err < 0) 
    {
      unsigned int minp, maxp;
      int dir;
#if HAVE_NEW_ALSA
      snd_pcm_hw_params_get_periods_min(hw_params, &minp, &dir);
      snd_pcm_hw_params_get_periods_max(hw_params, &maxp, &dir);
#else
      minp = snd_pcm_hw_params_get_periods_min(hw_params, &dir);
      maxp = snd_pcm_hw_params_get_periods_max(hw_params, &dir);
#endif
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: %s: cannot set number of periods to %d, min is %d, max is %d", 
				       snd_strerror(err), alsa_name, periods, (int)minp, (int)maxp)));
    }

  frames = size / chans / mus_bytes_per_sample(format);

  err = snd_pcm_hw_params_set_buffer_size(handle, hw_params, frames * periods);
  if (err < 0) 
    {
      snd_pcm_uframes_t minp, maxp;
#if HAVE_NEW_ALSA
      snd_pcm_hw_params_get_buffer_size_min(hw_params, &minp);
      snd_pcm_hw_params_get_buffer_size_max(hw_params, &maxp);
#else
      minp = snd_pcm_hw_params_get_buffer_size_min(hw_params);
      maxp = snd_pcm_hw_params_get_buffer_size_max(hw_params);
#endif
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: %s: cannot set buffer size to %d periods of %d frames; \
total requested buffer size is %d frames, minimum allowed is %d, maximum is %d", 
				       snd_strerror(err), alsa_name, periods, frames, periods * frames, (int)minp, (int)maxp)));
    }

  err = snd_pcm_hw_params_set_format(handle, hw_params, alsa_format);
  if (err < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: %s: cannot set format to %s", 
				       snd_strerror(err), alsa_name, snd_pcm_format_name(alsa_format))));
    }

  err = snd_pcm_hw_params_set_channels(handle, hw_params, chans);
  if (err < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: %s: cannot set channels to %d", 
				       snd_strerror(err), alsa_name, chans)));
    }
#if HAVE_NEW_ALSA
  {
    unsigned int new_rate;
    new_rate = srate;
    r = snd_pcm_hw_params_set_rate_near(handle, hw_params, &new_rate, 0);
    if (r < 0) 
      {
	snd_pcm_close(handle);
	handles[alsa_stream] = NULL;
	alsa_dump_configuration(alsa_name, hw_params, sw_params);
	return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			      mus_format("%s: %s: cannot set sampling rate near %d", 
					 snd_strerror(r), alsa_name, srate)));
      } 
    else 
      {
	if ((new_rate != srate) && (!alsa_squelch_warning))
	  {
	    mus_print("%s: could not set rate to exactly %d, set to %d instead",
		      alsa_name, srate, new_rate);
	  }
      }
  }
#else
  r = snd_pcm_hw_params_set_rate_near(handle, hw_params, srate, 0);
  if (r < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: %s: cannot set sampling rate near %d", 
				       snd_strerror(r), alsa_name, srate)));
    } 
  else 
    {
      if (r != srate) 
	{
	  mus_print("%s: could not set rate to exactly %d, set to %d instead",
		    alsa_name, srate, r);
	}
    }
#endif

  err = snd_pcm_hw_params(handle, hw_params);
  if (err < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: cannot set hardware parameters for %s", 
				       snd_strerror(err), alsa_name)));
    }

  snd_pcm_sw_params_current(handle, sw_params);
  err = snd_pcm_sw_params(handle, sw_params);
  if (err < 0) 
    {
      snd_pcm_close(handle);
      handles[alsa_stream] = NULL;
      alsa_dump_configuration(alsa_name, hw_params, sw_params);
      return(alsa_mus_error(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
			    mus_format("%s: cannot set software parameters for %s", 
				       snd_strerror(err), alsa_name)));
    }

  /* for now the id for the stream is the direction identifier, that is
     not a problem because we only advertise one card with two devices */
  return(alsa_stream);
}

/* sndlib support for opening output devices */

static int alsa_mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size)
{
  return(alsa_audio_open(ur_dev, srate, chans, format, size));
}

/* sndlib support for opening input devices */

static int alsa_mus_audio_open_input(int ur_dev, int srate, int chans, int format, int size)
{
  return(alsa_audio_open(ur_dev, srate, chans, format, size));
}

/* sndlib support for closing a device */

/* to force it to stop, snd_pcm_drop */

static bool xrun_warned = false;

static int alsa_mus_audio_close(int id)
{
  int err = 0;
  xrun_warned = false;
  if (alsa_trace) mus_print( "%s: %d", c__FUNCTION__, id); 
  if (handles[id]) 
    {
      err = snd_pcm_drain(handles[id]);
      if (err != 0) 
	mus_print("snd_pcm_drain: %s", snd_strerror(err)); 

      err = snd_pcm_close(handles[id]);
      if (err != 0) 
	return(alsa_mus_error(MUS_AUDIO_CANT_CLOSE, 
			      mus_format("snd_pcm_close: %s", 
					 snd_strerror(err)))); 
      handles[id] = NULL;
    }
  return(MUS_NO_ERROR);
}

/* recover from underruns or overruns */

static int recover_from_xrun(int id)
{
  int err;
  snd_pcm_status_t *status;
  snd_pcm_state_t state;
  snd_pcm_status_alloca(&status);
  err = snd_pcm_status(handles[id], status);
  if (err < 0) 
    {
      mus_print("%s: snd_pcm_status: %s", c__FUNCTION__, snd_strerror(err));
      return(MUS_ERROR);
    }
  state = snd_pcm_status_get_state(status);
  if (state == SND_PCM_STATE_XRUN) 
    {
      if (!xrun_warned)
	{
	  xrun_warned = true;
	  mus_print("[under|over]run detected");
	}
      err = snd_pcm_prepare(handles[id]);
      if (err < 0) 
	mus_print("snd_pcm_prepare: %s", snd_strerror(err));
      else return(MUS_NO_ERROR);
    }
  else mus_print("%s: error, current state is %s", c__FUNCTION__, snd_pcm_state_name(state));
  return(MUS_ERROR);
}

/* sndlib support for writing a buffer to an output device */

static int alsa_mus_audio_write(int id, char *buf, int bytes)
{
  snd_pcm_sframes_t status;
  ssize_t frames;
  frames = snd_pcm_bytes_to_frames(handles[id], bytes);
#if MUS_DEBUGGING
  if ((frames <= 0) || (frames > bytes))
    {
      /* pcm->frame_bits not correct? */
      mus_print("audio write %d frames (%d bytes)?", bytes, frames);
      abort();
      return(MUS_ERROR);
    }
#endif
  status = snd_pcm_writei(handles[id], buf, frames);
  if ((status == -EAGAIN) || 
      ((status >= 0) && (status < frames)))
    snd_pcm_wait(handles[id], 1000);
  else
    {
      if (status == -EPIPE) 
	return(recover_from_xrun(id));
      else 
	{
	  if (status < 0) 
	    {
	      mus_print("snd_pcm_writei: %s", snd_strerror(status));
	      return(MUS_ERROR);
	    }
	}
    }
  return(MUS_NO_ERROR);
}

/* sndlib support for reading a buffer from an input device */

static int alsa_mus_audio_read(int id, char *buf, int bytes)
{
  snd_pcm_sframes_t status;
  ssize_t frames;
  frames = snd_pcm_bytes_to_frames(handles[id], bytes);
#if MUS_DEBUGGING
  if ((frames <= 0) || (frames > bytes))
    {
      mus_print("audio read %d frames (%d bytes)?", frames, bytes);
      abort();
      return(MUS_ERROR);
    }
#endif
  status = snd_pcm_readi(handles[id], buf, frames);
  if ((status == -EAGAIN) || 
      ((status >= 0) && (status < frames)))
    snd_pcm_wait(handles[id], 1000);
  else 
    {
      if (status == -EPIPE) 
	return(recover_from_xrun(id));
      else 
	{
	  if (status < 0) 
	    {
	      mus_print("snd_pcm_readi: %s", snd_strerror(status));
	      return(MUS_ERROR);
	    }
	}
    }
  return(MUS_NO_ERROR);
}

/* read state of the audio hardware */

static int alsa_mus_audio_mixer_read(int ur_dev, int field, int chan, float *val)
{
  int card;
  int device;
  int alsa_device;
  snd_pcm_stream_t alsa_stream;
  int f, err;
  
  if ((!audio_initialized) && 
      (mus_audio_initialize() != MUS_NO_ERROR))
    return(MUS_ERROR);
  
  card = MUS_AUDIO_SYSTEM(ur_dev);
  device = MUS_AUDIO_DEVICE(ur_dev);
  if (alsa_trace) 
    mus_print( "%s: card=%d, dev=%s<%d>, field=%s<%d>, chan=%d",
	       c__FUNCTION__, card, mus_audio_device_name(device), device, 
	       mus_audio_device_name(field), field, 
	       chan);
  /* for now do not implement mixer interface */
  if (device == MUS_AUDIO_MIXER) 
    {
      val[0] = 0;
      return(MUS_NO_ERROR);
    }
  /* MUS_AUDIO_PORT probes for devices and should not depend on the
   * device which was used in the ur_dev argument, we process this
   * before trying to map the device to an alsa device */
  
  if (field == MUS_AUDIO_PORT) 
    {
      /* under 0.9 we only advertise at most two devices, one for playback 
	 and another one for capture */
      /* int dev;  */
      int i = 1;
      if (alsa_hw_params[SND_PCM_STREAM_PLAYBACK]) 
	val[i++] = (float)to_sndlib_device(0, SND_PCM_STREAM_PLAYBACK);

      if (alsa_hw_params[SND_PCM_STREAM_CAPTURE]) 
	val[i++] = (float)to_sndlib_device(0, SND_PCM_STREAM_CAPTURE);

      val[0]=(float)(i - 1);
      return(MUS_NO_ERROR);
    }
  /* map the mus device to an alsa device and channel */
  if ((err = to_alsa_device(device, &alsa_device, &alsa_stream)) < 0) 
    {
      /* FIXME: snd-dac still probes some non-existing devices, specifically
       * MUS_AUDIO_DAC_FILTER, do not report error till that's fixed */
      if (alsa_trace) 
	{
	  mus_print("%s: cannot translate device %s<%d> to alsa, field=%s<%d>",
		    snd_strerror(err),
		    mus_audio_device_name(device), device, 
		    mus_audio_device_name(field), field);
	}
      return(MUS_ERROR);
    }
  if (alsa_trace) mus_print("%s:         adev=%d, achan=%d", c__FUNCTION__, alsa_device, alsa_stream);
  switch (field) 
    {
    case MUS_AUDIO_AMP: 
      /* amplitude value */
      val[0] = 1.0;
      break;
    case MUS_AUDIO_SAMPLES_PER_CHANNEL: 
      /* samples per channel */
      if (card > 0 || alsa_device > 0) 
	return(alsa_mus_error(MUS_AUDIO_CANT_READ, NULL));
      else 
	{
	  val[0] = (float)alsa_samples_per_channel;
	  if (chan > 1) 
	    {
#if HAVE_NEW_ALSA
	      snd_pcm_uframes_t tmp = 0;
	      snd_pcm_hw_params_get_buffer_size_min(alsa_hw_params[alsa_stream], &tmp); 
	      val[1] = (float)tmp;
	      snd_pcm_hw_params_get_buffer_size_max(alsa_hw_params[alsa_stream], &tmp); 
	      val[2] = (float)tmp;
#else
	      val[1] = (float)snd_pcm_hw_params_get_buffer_size_min(alsa_hw_params[alsa_stream]); 
	      val[2] = (float)snd_pcm_hw_params_get_buffer_size_max(alsa_hw_params[alsa_stream]); 
#endif
	    }
	}
      break;
    case MUS_AUDIO_CHANNEL: 
      /* number of channels */
      if (card > 0 || alsa_device > 0) 
	return(alsa_mus_error(MUS_AUDIO_CANT_READ, NULL));
      else 
	{

	  if ((alsa_stream == SND_PCM_STREAM_CAPTURE) &&
	      (alsa_capture_device_name) &&
	      (strcmp(alsa_capture_device_name, "default") == 0))
	    {
	      val[0] = 2;
	    }
	  else
	    {

#if HAVE_NEW_ALSA
	      unsigned int max_channels = 0;
	      snd_pcm_hw_params_get_channels_max(alsa_hw_params[alsa_stream], &max_channels);
#else
	      int max_channels = snd_pcm_hw_params_get_channels_max(alsa_hw_params[alsa_stream]);
#endif
	      if ((alsa_stream == SND_PCM_STREAM_CAPTURE) &&
		  (max_channels > alsa_max_capture_channels))
		{
		  /* limit number of capture channels to a reasonable maximum, if the user
		     specifies a plug pcm as the capture pcm then the returned number of channels
		     would be MAXINT (or whatever the name is for a really big number). At this
		     point there is no support in the alsa api to distinguish between default
		     parameters or those that have been set by a user on purpose, of for querying
		     the hardware pcm device that is hidden by the plug device to see what is the
		     real number of channels for the device we are dealing with. We could also try
		     to flag this as an error to the user and exit the program */
		  max_channels = alsa_max_capture_channels;
		}
	      val[0] = (float)max_channels;
	      if (chan > 1) 
		{
#if HAVE_NEW_ALSA
		  unsigned int tmp = 0;
		  snd_pcm_hw_params_get_channels_min(alsa_hw_params[alsa_stream], &tmp); 
		  val[1] = (float)tmp;
#else
		  val[1] = (float)snd_pcm_hw_params_get_channels_min(alsa_hw_params[alsa_stream]); 
#endif
		  val[2] = (float)max_channels;
		}
	    }
	}
      break;
    case MUS_AUDIO_SRATE: 
      /* supported sample rates */
      if (card > 0 || alsa_device > 0) 
	return(alsa_mus_error(MUS_AUDIO_CANT_READ, NULL));
      else 
	{
	  int dir = 0;
	  val[0] = 44100;
	  if (chan > 1) 
	    {
#if HAVE_NEW_ALSA
	      unsigned int tmp;
	      snd_pcm_hw_params_get_rate_min(alsa_hw_params[alsa_stream], &tmp, &dir); 
	      val[1] = (float)tmp;
	      snd_pcm_hw_params_get_rate_max(alsa_hw_params[alsa_stream], &tmp, &dir); 
	      val[2] = (float)tmp;
#else
	      val[1] = (float)snd_pcm_hw_params_get_rate_min(alsa_hw_params[alsa_stream], &dir); 
	      val[2] = (float)snd_pcm_hw_params_get_rate_max(alsa_hw_params[alsa_stream], &dir); 
#endif
	    }
	}
      break;
    case MUS_AUDIO_FORMAT:
      /* supported formats */
      if (card > 0 || alsa_device > 0) 
	return(alsa_mus_error(MUS_AUDIO_CANT_READ, NULL));
      else 
	{
	  int format;
	  snd_pcm_format_mask_t *mask;
	  snd_pcm_format_mask_alloca(&mask);
	  snd_pcm_hw_params_get_format_mask(alsa_hw_params[alsa_stream], mask); 
	  for (format = 0, f = 1; format < SND_PCM_FORMAT_LAST; format++) 
	    {
	      err = snd_pcm_format_mask_test(mask, (snd_pcm_format_t)format);
	      if (err > 0) 
		{
		  if ((f < chan) && 
		      (to_mus_format(format)!=MUS_ERROR))
		    val[f++] = (float)to_mus_format(format);
		}
	    }
	  val[0] = f - 1;
	}
      break;                
    case MUS_AUDIO_DIRECTION: 
      /* direction of this device */
      if (card > 0 || alsa_device > 0) 
	return(alsa_mus_error(MUS_AUDIO_CANT_READ, NULL));
      else 
	{
	  /* 0-->playback, 1-->capture */
	  val[0] = (float)alsa_stream;
	}
      break;
    default: 
      return(alsa_mus_error(MUS_AUDIO_CANT_READ, NULL));
      break;
    }
  return(MUS_NO_ERROR);
}

static int alsa_mus_audio_mixer_write(int ur_dev, int field, int chan, float *val)
{
  return(MUS_NO_ERROR);
}

static void alsa_describe_audio_state_1(void)
{
  int err; 
  char *str;
  size_t len;
  snd_config_t *conf;
  snd_output_t *buf = NULL;
#if (SND_LIB_MAJOR == 0) || ((SND_LIB_MAJOR == 1) && (SND_LIB_MINOR == 0) && (SND_LIB_SUBMINOR < 8))
  return; /* avoid Alsa bug */
#endif
  err = snd_config_update();
  if (err < 0) 
    {
      mus_print("snd_config_update: %s", snd_strerror(err));
      return;
    }
  err = snd_output_buffer_open(&buf);
  if (err < 0) 
    mus_print("could not open dump buffer: %s", snd_strerror(err));
  else 
    {
      err = snd_config_search(snd_config, "pcm", &conf);
      if (err < 0) 
	{
	  mus_print("snd_config_search: could not find at least one pcm: %s", snd_strerror(err));
	  return;
	}
      snd_output_puts(buf, "PCM list:\n");
      snd_config_save(conf, buf);
      snd_output_putc(buf, '\0');
      len = snd_output_buffer_string(buf, &str);
      if (len > 1) 
	pprint(str);
      snd_output_close(buf);
    }
}

#endif /* HAVE_ALSA */


/* -------------------------------- SUN -------------------------------- */
/*
 * Thanks to Seppo Ingalsuo for several bugfixes.
 * record case improved after perusal of Snack 1.6/src/jkAudio_sun.c
 */

/* apparently input other than 8000 is 16-bit, 8000 is (?) mulaw */

#if (defined(MUS_SUN) || defined(MUS_OPENBSD)) && (!(defined(AUDIO_OK)))
#define AUDIO_OK

#include <sys/types.h>
#include <stropts.h>
#include <sys/filio.h>

#ifdef SUNOS
#include <sun/audioio.h>
#else
#include <sys/audioio.h>
#endif
#if HAVE_SYS_MIXER_H
#include <sys/mixer.h>
#endif

int mus_audio_initialize(void) {return(MUS_NO_ERROR);}
int mus_audio_systems(void) {return(1);}
char *mus_audio_system_name(int system) {return("Sun");}

static int sun_default_outputs = (AUDIO_HEADPHONE | AUDIO_LINE_OUT | AUDIO_SPEAKER);

void mus_sun_set_outputs(int speakers, int headphones, int line_out)
{
  sun_default_outputs = 0;
  if (speakers) sun_default_outputs |= AUDIO_SPEAKER;
  if (headphones) sun_default_outputs |= AUDIO_HEADPHONE;
  if (line_out) sun_default_outputs |= AUDIO_LINE_OUT;
}


#ifdef MUS_OPENBSD
  #define DAC_NAME "/dev/sound"
#else
  #define DAC_NAME "/dev/audio"
#endif
#define AUDIODEV_ENV "AUDIODEV"

#define RETURN_ERROR_EXIT(Error_Type, Audio_Line, Ur_Error_Message) \
  do { char *Error_Message; Error_Message = Ur_Error_Message; \
    if (Audio_Line != -1) close(Audio_Line); \
    if (Error_Message) \
      {MUS_STANDARD_ERROR(Error_Type, Error_Message); FREE(Error_Message);} \
    else MUS_STANDARD_ERROR(Error_Type, mus_error_type_to_string(Error_Type)); \
    return(MUS_ERROR); \
  } while (false)

char *mus_audio_moniker(void) 
{
#ifndef AUDIO_DEV_AMD
  struct audio_device ad;
#else
  int ad;
#endif
  int audio_fd, err;
  char *dev_name;
  if (getenv(AUDIODEV_ENV) != NULL) 
    dev_name = getenv(AUDIODEV_ENV); 
  else dev_name = DAC_NAME;
  audio_fd = open(dev_name, O_RDONLY | O_NONBLOCK, 0);
  if (audio_fd == -1) 
    {
      audio_fd = open("/dev/audioctl", O_RDONLY | O_NONBLOCK, 0);
      if (audio_fd == -1) return("sun probably");
    }
  err = ioctl(audio_fd, AUDIO_GETDEV, &ad); 
  if (err == -1) 
    {
      close(audio_fd); 
      return("sun?");
    }
  mus_audio_close(audio_fd);
#if HAVE_SYS_MIXER_H
  if (version_name == NULL) version_name = (char *)CALLOC(PRINT_BUFFER_SIZE, sizeof(char));
#else
  if (version_name == NULL) version_name = (char *)CALLOC(LABEL_BUFFER_SIZE, sizeof(char));
#endif
#ifndef AUDIO_DEV_AMD
  #if HAVE_SYS_MIXER_H
    mus_snprintf(version_name, PRINT_BUFFER_SIZE, 
		 "audio: %s (%s), %s %s %s", 
		 ad.name, ad.version,
		 MIXER_NAME, MIXER_VERSION, MIXER_CONFIGURATION);
  #else
    mus_snprintf(version_name, LABEL_BUFFER_SIZE, "audio: %s (%s)", ad.name, ad.version);
  #endif
#else
  switch (ad)
    {
    case AUDIO_DEV_AMD:        mus_snprintf(version_name, LABEL_BUFFER_SIZE, "audio: amd");        break;
  #ifdef AUDIO_DEV_CS4231
    case AUDIO_DEV_CS4231:     mus_snprintf(version_name, LABEL_BUFFER_SIZE, "audio: cs4231");     break;
  #endif
    case AUDIO_DEV_SPEAKERBOX: mus_snprintf(version_name, LABEL_BUFFER_SIZE, "audio: speakerbox"); break;
    case AUDIO_DEV_CODEC:      mus_snprintf(version_name, LABEL_BUFFER_SIZE, "audio: codec");      break;
    default:                   mus_snprintf(version_name, LABEL_BUFFER_SIZE, "audio: unknown");    break;
    }
#endif
  return(version_name);
}

static int to_sun_format(int format)
{
  switch (format)
    {
#if MUS_LITTLE_ENDIAN
    case MUS_LSHORT: /* Solaris on Intel? */
#else
    case MUS_BSHORT: 
#endif
#ifdef MUS_OPENBSD
      return(AUDIO_ENCODING_PCM16); 
#else
      return(AUDIO_ENCODING_LINEAR); 
#endif
      break;
    case MUS_BYTE: 
#if defined(AUDIO_ENCODING_LINEAR8)
      return(AUDIO_ENCODING_LINEAR8); break;
#else
  #ifdef MUS_OPENBSD
      return(AUDIO_ENCODING_PCM8); 
  #else
      return(AUDIO_ENCODING_LINEAR);
  #endif 
      break;
#endif
    case MUS_MULAW: return(AUDIO_ENCODING_ULAW); break;
    case MUS_ALAW:  return(AUDIO_ENCODING_ALAW); break;
      /* there's also AUDIO_ENCODING_DVI */
    }
  return(MUS_ERROR);
}

int mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size)
{
  struct audio_info info;
  char *dev_name;
  int encode, bits, dev;
  int audio_fd, err;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  encode = to_sun_format(format);
  if (encode == MUS_ERROR) 
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d (%s) not available",
				 format, 
				 mus_data_format_name(format)));
  if (getenv(AUDIODEV_ENV) != NULL) 
    dev_name = getenv(AUDIODEV_ENV); 
  else dev_name = DAC_NAME;
  if (dev != MUS_AUDIO_DUPLEX_DEFAULT)
    audio_fd = open(dev_name, O_WRONLY, 0);
  else audio_fd = open(dev_name, O_RDWR, 0);
  if (audio_fd == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, -1,
		      mus_format("can't open output %s: %s",
				 dev_name, strerror(errno)));
  AUDIO_INITINFO(&info);
  if (dev == MUS_AUDIO_LINE_OUT)
    info.play.port = AUDIO_LINE_OUT;
  else
    {
      if (dev == MUS_AUDIO_SPEAKERS)
	/* OR may not be available */
	info.play.port = AUDIO_SPEAKER | (sun_default_outputs & AUDIO_HEADPHONE);
      else 
	info.play.port = sun_default_outputs;
    }
  info.play.sample_rate = srate; 
  info.play.channels = chans;
  bits = 8 * mus_bytes_per_sample(format);
  info.play.precision = bits;
  info.play.encoding = encode;
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    {
      ioctl(audio_fd, AUDIO_GETINFO, &info); 

      if ((int)info.play.channels != chans) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
			  mus_format("can't set output %s (%s) channels to %d",
				     mus_audio_device_name(dev), dev_name, chans));
      
      if (((int)info.play.precision != bits) || 
	  ((int)info.play.encoding != encode)) 
	RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_fd,
			  mus_format("can't set output %s (%s) format to %d bits, %d encode (%s)",
				     mus_audio_device_name(dev), dev_name,
				     bits, encode, 
				     mus_data_format_name(format)));
      
      if ((int)info.play.sample_rate != srate) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
			  mus_format("can't set output %s (%s) srate to %d",
				     mus_audio_device_name(dev), dev_name, srate));
    }
  /* man audio sez the play.buffer_size field is not currently supported */
  /* but since the default buffer size is 8180! we need ioctl(audio_fd, I_SETSIG, ...) */
  ioctl(audio_fd, I_FLUSH, FLUSHR);
  return(audio_fd);
}

int mus_audio_write(int line, char *buf, int bytes)
{
  if (write(line, buf, bytes) != bytes) 
    RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
		      mus_format("write error: %s", strerror(errno)));
  return(MUS_NO_ERROR);
}

int mus_audio_close(int line)
{
  write(line, (char *)NULL, 0);
  close(line);
  return(MUS_NO_ERROR);
}

int mus_audio_read(int line, char *buf, int bytes)
{
  int total = 0;
  char *curbuf;
  /* ioctl(line, AUDIO_DRAIN, NULL) */
  /* this seems to return 8-12 bytes fewer than requested -- perverse! */
  /* should I buffer data internally? */

  /* apparently we need to loop here ... */
  curbuf = buf;
  while (total < bytes)
    {
      int bytes_available;
      ioctl(line, FIONREAD, &bytes_available);
      if (bytes_available > 0)
	{
	  int bytes_read;
	  if ((total + bytes_available) > bytes) bytes_available = bytes - total;
	  bytes_read = read(line, curbuf, bytes_available);
	  if (bytes_read > 0)
	    {
	      total += bytes_read;
	      curbuf = (char *)(buf + total);
	    }
	  /* else return anyway?? */
	}
    }
  return(MUS_NO_ERROR);
}

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int size)
{
  struct audio_info info;
  int indev, encode, bits, dev, audio_fd, err;
  char *dev_name;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  encode = to_sun_format(format);
  bits = 8 * mus_bytes_per_sample(format);
  if (encode == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d bits, %d encode (%s) not available",
				 bits, encode, 
				 mus_data_format_name(format)));
  if (getenv(AUDIODEV_ENV) != NULL) 
    dev_name = getenv(AUDIODEV_ENV); 
  else dev_name = DAC_NAME;
  if (dev != MUS_AUDIO_DUPLEX_DEFAULT)
    audio_fd = open(dev_name, O_RDONLY, 0);
  else audio_fd = open(dev_name, O_RDWR, 0);
  if (audio_fd == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, -1,
		      mus_format("can't open input %s: %s",
				 dev_name, strerror(errno)));
  AUDIO_INITINFO(&info);
  /*  ioctl(audio_fd, AUDIO_GETINFO, &info); */
  info.record.sample_rate = srate;
  info.record.channels = chans;
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, audio_fd,
		      mus_format("can't set srate %d and chans %d for input %s (%s)",
				 srate, chans,
				 dev_name, 
				 mus_audio_device_name(dev)));
  ioctl(audio_fd, AUDIO_GETINFO, &info);
  if (info.record.sample_rate != (unsigned int)srate) 
    mus_print("%s[%d]: sampling rate: %d != %d\n", 
	      __FILE__, __LINE__, 
	      info.record.sample_rate, srate);
  if (info.record.channels != (unsigned int)chans) 
    mus_print("%s[%d]: channels: %d != %d\n", 
	      __FILE__, __LINE__, 
	      info.record.channels, chans);

  info.record.precision = bits; /* was play, changed 10-Jul-03 thanks to Jürgen Keil */
  info.record.encoding = encode;
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, audio_fd,
		      mus_format("can't set bits %d, encode %d (format %s) for input %s (%s)",
				 bits, encode, mus_data_format_name(format),
				 dev_name, 
				 mus_audio_device_name(dev)));
  ioctl(audio_fd, AUDIO_GETINFO, &info);

  /* these cannot be OR'd */
  if (dev == MUS_AUDIO_LINE_IN) 
    indev = AUDIO_LINE_IN; 
  else
    {
      if (dev == MUS_AUDIO_CD) 
	indev = AUDIO_INTERNAL_CD_IN;
      else indev = AUDIO_MICROPHONE;
    }
  info.record.port = indev;
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
		      mus_format("can't set record.port to %d for %s (%s)",
				 indev, dev_name, 
				 mus_audio_device_name(dev)));
  err = ioctl(audio_fd, AUDIO_GETINFO, &info);
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't getinfo on input %s (%s, line: %d)",
				 dev_name, 
				 mus_audio_device_name(dev), audio_fd));
  else 
    {
      if ((int)info.record.port != indev) 
	RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, audio_fd,
			  mus_format("confusion in record.port: %d != %d (%s: %s)",
				     (int)info.record.port, indev,
				     dev_name, 
				     mus_audio_device_name(dev)));
      if ((int)info.record.channels != chans) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_fd,
			  mus_format("confusion in record.channels: %d != %d (%s: %s)",
				     (int)info.record.channels, chans,
				     dev_name, 
				     mus_audio_device_name(dev)));
      if (((int)info.record.precision != bits) || 
	  ((int)info.record.encoding != encode)) 
	RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_fd,
			  mus_format("confusion in record.precision|encoding: %d != %d or %d != %d (%s: %s)",
				     (int)info.record.precision, bits,
				     (int)info.record.encoding, encode,
				     dev_name, 
				     mus_audio_device_name(dev)));
    }
  /* this may be a bad idea */
  info.record.buffer_size = size;
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
		      mus_format("can't set buffer size to %d on input %s (%s)",
				 size,
				 dev_name, 
				 mus_audio_device_name(dev)));
  return(audio_fd);
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val) 
{
#ifndef AUDIO_DEV_AMD
  struct audio_device ad;
#else 
  int ad;
#endif
  int audio_fd, err;
  struct audio_info info;
  int dev, port;
  char *dev_name;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  AUDIO_INITINFO(&info);
  if (getenv(AUDIODEV_ENV) != NULL) 
    dev_name = getenv(AUDIODEV_ENV); 
  else dev_name = DAC_NAME;
  audio_fd = open(dev_name, O_RDONLY | O_NONBLOCK, 0);
  if (audio_fd == -1) 
    {
      audio_fd = open("/dev/audioctl", O_RDONLY | O_NONBLOCK);
      if (audio_fd == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1,
			  mus_format("can't open %s or /dev/audioctl: %s",
				     dev_name, strerror(errno)));
      else dev_name = "/dev/audioctl";
    }
  err = ioctl(audio_fd, AUDIO_GETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't get %s (%s) info",
				 dev_name, 
				 mus_audio_device_name(dev)));
  if (field == MUS_AUDIO_PORT)
    {
      /* info.play|record have a field avail_ports */
      port = 1;
      if ((chan > port) && 
	  (info.record.avail_ports & AUDIO_MICROPHONE)) 
	{
	  val[port] = MUS_AUDIO_MICROPHONE; 
	  port++;
	}
      if ((chan > port) && 
	  (info.record.avail_ports & AUDIO_LINE_IN)) 
	{
	  val[port] = MUS_AUDIO_LINE_IN; 
	  port++;
	}
#ifndef AUDIO_DEV_AMD
      if ((chan > port) && 
	  (info.record.avail_ports & AUDIO_INTERNAL_CD_IN)) 
	{
	  /* this field lies -- there is no such port available on the Ultra */
	  err = ioctl(audio_fd, AUDIO_GETDEV, &ad); 
	  if (err == -1) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
			      mus_format("can't get device info on %s (%s)",
					 dev_name, 
					 mus_audio_device_name(dev)));
	  if (((ad.version) && (strcmp(ad.version, "a") == 0)) || /* is it a SparcStation? */
	      ((ad.name) && (strcmp(ad.name, "SUNW,CS4231") == 0)))
	    {
	      val[port] = MUS_AUDIO_CD; 
	      port++;
	    }
	}
#endif
      if ((chan > port) && 
	  (info.play.avail_ports & AUDIO_SPEAKER)) 
	{
	  val[port] = MUS_AUDIO_SPEAKERS; 
	  port++;
	}
      if ((chan > port) &&
	  (info.play.avail_ports & AUDIO_LINE_OUT)) 
	{
	  val[port] = MUS_AUDIO_LINE_OUT; 
	  port++;
	}
      if ((chan > port) && 
	  (info.play.avail_ports & AUDIO_HEADPHONE)) 
	{
	  val[port] = MUS_AUDIO_DAC_OUT; 
	  port++;
	}
      val[0] = port - 1;
    }
  else
    {
      if (field == MUS_AUDIO_FORMAT)  /* this actually depends on the audio device */
        {
	  err = ioctl(audio_fd, AUDIO_GETDEV, &ad); /* SUNW, dbri|am79c30|CS4231|sbpro|sb16 */
	  /* Jurgen Keil's drivers use SUNW,CS4231, but the "real" names are:
	     "TOOLS,sbpci"           SoundBlaster PCI card
	     "TOOLS,EMU10Kx"         SoundBlaster Live! or Audigy
	     "TOOLS,i810"            Intel i8xx audio (and compatible)
	     "TOOLS,via686"          VIA 686 audio
	     "TOOLS,via8233"         VIA 8233 (and compatible)
	  */
	  if (err == -1) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
			      mus_format("can't get data format info for %s (%s)",
					 dev_name, 
					 mus_audio_device_name(dev)));
	  port = 1;
	  if ((ad.name) &&
	      (strcmp(ad.name, "SUNW,audio810") == 0))
	    {
	      val[0] = 1;
	      val[1] = MUS_MULAW;
	    }
	  else
	    {
#ifndef AUDIO_DEV_AMD
	      if ((ad.name) && 
		  (strcmp(ad.name, "SUNW, am79c30") != 0))
#else
	      if (ad == AUDIO_DEV_AMD)
#endif
		{
		  if (chan > port) val[port++] = MUS_BSHORT; 
		}
#ifndef AUDIO_DEV_AMD
	      if ((ad.name) && 
		  (strcmp(ad.name, "SUNW, sbpro") != 0) && 
		  (strcmp(ad.name, "SUNW, sb16") != 0))
		{
		  if (chan > port) val[port++] = MUS_ALAW; 
		}
#endif
	      if (chan > port) val[port++] = MUS_MULAW;
#if MUS_LITTLE_ENDIAN
	      if (chan > port) val[port++] = MUS_LSHORT;
#endif
	      val[0] = port - 1;
	    }
        }
      else
        {
          switch (dev)
            {
            case MUS_AUDIO_DEFAULT:
            case MUS_AUDIO_DAC_OUT:
            case MUS_AUDIO_SPEAKERS:
            case MUS_AUDIO_LINE_OUT:
              switch (field)
                {
                case MUS_AUDIO_AMP: 
		  /* who knows how this really works?  documentation is incomplete, actual behavior seems to be: */
		  if (chan == 0)
		    {
		      if (info.play.balance <= (AUDIO_RIGHT_BALANCE / 2))
			val[0] = info.play.gain / (float)(AUDIO_MAX_GAIN);
		      else val[0] = info.play.gain * (AUDIO_RIGHT_BALANCE - info.play.balance) / (float)(AUDIO_MAX_GAIN * (AUDIO_RIGHT_BALANCE / 2));
		    }
		  else
		    {
		      if (info.play.balance >= (AUDIO_RIGHT_BALANCE / 2))
			val[0] = info.play.gain / (float)(AUDIO_MAX_GAIN);
		      else val[0] = info.play.gain * info.play.balance / (float)(AUDIO_MAX_GAIN * (AUDIO_RIGHT_BALANCE / 2));
		    }
		  break;
                case MUS_AUDIO_CHANNEL: 
		  val[0] = 2; 
		  break;
		  /* appears to depend on data format (mulaw is mono) */
                case MUS_AUDIO_SRATE: 
		  val[0] = (float)info.play.sample_rate; 
		  break;
                default: 
		  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd, 
				    mus_format("can't read %s field %d (%s)",
					       mus_audio_device_name(dev),
					       field, 
					       mus_audio_device_name(field)));
		  break;
                }
              break;
            case MUS_AUDIO_MICROPHONE:
            case MUS_AUDIO_LINE_IN:
            case MUS_AUDIO_DUPLEX_DEFAULT:
	    case MUS_AUDIO_CD:
              switch (field)
                {
                case MUS_AUDIO_AMP:
		  if (chan == 0)
		    {
		      if (info.record.balance <= (AUDIO_RIGHT_BALANCE / 2))
			val[0] = info.record.gain / (float)(AUDIO_MAX_GAIN);
		      else val[0] = info.record.gain * (AUDIO_RIGHT_BALANCE - info.record.balance) / (float)(AUDIO_MAX_GAIN * (AUDIO_RIGHT_BALANCE / 2));
		    }
		  else
		    {
		      if (info.record.balance >= (AUDIO_RIGHT_BALANCE / 2))
			val[0] = info.record.gain / (float)(AUDIO_MAX_GAIN);
		      else val[0] = info.record.gain * info.record.balance / (float)(AUDIO_MAX_GAIN * (AUDIO_RIGHT_BALANCE / 2));
		    }
		  break;

                case MUS_AUDIO_CHANNEL: 
		  val[0] = 1; 
		  break;
                case MUS_AUDIO_SRATE: 
		  val[0] = (float)(info.record.sample_rate); 
		  break;
		case MUS_AUDIO_IGAIN: 
		  val[0] = (float)(info.monitor_gain) / (float)AUDIO_MAX_GAIN; 
		  break;
                default: 
		  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
					mus_format("can't read %s field %d (%s)",
						   mus_audio_device_name(dev),
						   field, 
						   mus_audio_device_name(field)));
		  break;
                }
              break;
            default: 
	      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
				mus_format("can't read %s field %d (%s)",
					   mus_audio_device_name(dev),
					   field, 
					   mus_audio_device_name(field)));
	      break;
            }
        }
    }
  return(mus_audio_close(audio_fd));
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val) 
{
  struct audio_info info;
  int dev, balance, gain;
  float ratio, lc, rc;
  int audio_fd, err;
  char *dev_name;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  AUDIO_INITINFO(&info);
  if (getenv(AUDIODEV_ENV) != NULL) 
    dev_name = getenv(AUDIODEV_ENV);
  else dev_name = DAC_NAME;
  audio_fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
  if (audio_fd == -1) 
    {
      audio_fd = open("/dev/audioctl", O_RDWR | O_NONBLOCK);
      if (audio_fd == -1) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, -1,
			  mus_format("can't write fields of %s (or /dev/audioctl) (%s): %s",
				     dev_name, 
				     mus_audio_device_name(dev),
				     strerror(errno)));
      else dev_name = "/dev/audioctl";
    }
  err = ioctl(audio_fd, AUDIO_GETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't get %s (%s) info",
				 dev_name, 
				 mus_audio_device_name(dev)));
  switch (dev)
    {
    case MUS_AUDIO_DEFAULT:
    case MUS_AUDIO_DAC_OUT:
    case MUS_AUDIO_SPEAKERS:
    case MUS_AUDIO_LINE_OUT:
      switch (field)
        {
        case MUS_AUDIO_AMP: 
	  balance = info.play.balance;
	  gain = info.play.gain;
	  if (balance <= (AUDIO_RIGHT_BALANCE / 2))
	    {
	      lc = gain;
	      rc = gain * balance / (float)(AUDIO_RIGHT_BALANCE / 2);
	    }
	  else
	    {
	      lc = gain * (AUDIO_RIGHT_BALANCE - balance) / (float)(AUDIO_RIGHT_BALANCE / 2);
	      rc = gain;
	    }
	  if (chan == 0)
	    lc = AUDIO_MAX_GAIN * val[0];
	  else rc = AUDIO_MAX_GAIN * val[0];
	  if ((rc + lc) == 0)
	    info.play.gain = 0;
	  else
	    {
	      ratio = (float)rc / (float)(rc + lc);
	      info.play.balance = (unsigned char)(AUDIO_RIGHT_BALANCE * ratio);
	      if (rc > lc) 
		info.play.gain = (int)rc;
	      else info.play.gain = (int)lc;
	    }
	  break;
        case MUS_AUDIO_CHANNEL: 
	  info.play.channels = (int)val[0]; 
	  break;
	  /* amd device only mono */
        case MUS_AUDIO_SRATE: 
	  info.play.sample_rate = (int)val[0]; 
	  break;
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
			    mus_format("can't write %s field %d (%s)",
				       mus_audio_device_name(dev),
				       field, 
				       mus_audio_device_name(field)));
	  break;
        }
      break;
    case MUS_AUDIO_MICROPHONE:
      switch (field)
	{
        case MUS_AUDIO_AMP: 
	  info.record.gain = (int)(AUDIO_MAX_GAIN * val[0]);
	  info.record.balance = 0;
	  break;
        case MUS_AUDIO_CHANNEL: 
	  info.record.channels = (int)val[0];
	  break;
        case MUS_AUDIO_SRATE: 
	  info.record.sample_rate = (int)val[0];
	  break;
	case MUS_AUDIO_IGAIN: 
	  info.monitor_gain = (int)(AUDIO_MAX_GAIN * val[0]); 
	  break;
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
			    mus_format("can't write %s field %d (%s)",
				       mus_audio_device_name(dev),
				       field, 
				       mus_audio_device_name(field)));
	  break;
	}
      break;
    case MUS_AUDIO_LINE_IN:
    case MUS_AUDIO_DUPLEX_DEFAULT:
    case MUS_AUDIO_CD:
      switch (field)
        {
        case MUS_AUDIO_AMP: 
	  balance = info.record.balance;
	  gain = info.record.gain;
	  lc = gain * (float)(AUDIO_RIGHT_BALANCE - balance) / (float)AUDIO_RIGHT_BALANCE;
	  rc = gain - lc;
	  if (chan == 0)
	    lc = AUDIO_MAX_GAIN * val[0];
	  else rc = AUDIO_MAX_GAIN * val[0];
	  gain = (int)(rc + lc);
	  if (gain == 0)
	    info.record.gain = 0;
	  else
	    {
	      info.record.balance = (unsigned char)(AUDIO_RIGHT_BALANCE * ((float)rc / (float)(rc + lc)));
	      if (rc > lc) 
		info.record.gain = (int)rc;
	      else info.record.gain = (int)lc;
	    }
	  break;
        case MUS_AUDIO_CHANNEL: 
	  info.record.channels = (int)val[0]; 
	  break;
        case MUS_AUDIO_SRATE: 
	  info.record.sample_rate = (int)val[0]; 
	  break;
	case MUS_AUDIO_IGAIN: 
	  info.monitor_gain = (int)(AUDIO_MAX_GAIN * val[0]); 
	  break;
        default: 
	  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
			    mus_format("can't write %s field %d (%s)",
				       mus_audio_device_name(dev),
				       field, 
				       mus_audio_device_name(field)));
	  break;
        }
      break;
    default: 
      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd, 
			mus_format("can't write %s field %d (%s)",
				   mus_audio_device_name(dev),
				   field, 
				   mus_audio_device_name(field)));
      break;
    }
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
		      mus_format("can't write %s field %d (%s) after explicit set",
				 mus_audio_device_name(dev),
				 field, 
				 mus_audio_device_name(field)));
  return(mus_audio_close(audio_fd));
}

/* pause can be implemented with play.pause and record.pause */

static char *sun_format_name(int format)
{
  switch (format)
    {
#ifdef AUDIO_ENCODING_ALAW
    case AUDIO_ENCODING_ALAW: return("alaw"); break;
#endif
#ifdef AUDIO_ENCODING_ULAW
    case AUDIO_ENCODING_ULAW: return("ulaw"); break;
#endif
#ifdef AUDIO_ENCODING_DVI
    case AUDIO_ENCODING_DVI: return("dvi adpcm"); break;
#endif
#ifdef AUDIO_ENCODING_LINEAR8
    case AUDIO_ENCODING_LINEAR8: return("linear"); break;
#else
  #ifdef AUDIO_ENCODING_PCM8
    case AUDIO_ENCODING_PCM8: return("linear"); break;
  #endif
#endif
#ifdef AUDIO_ENCODING_LINEAR
    case AUDIO_ENCODING_LINEAR: return("linear"); break;
#else
  #ifdef AUDIO_ENCODING_PCM16
    case AUDIO_ENCODING_PCM16: return("linear"); break;
  #endif
#endif
#ifdef AUDIO_ENCODING_NONE
    case AUDIO_ENCODING_NONE: return("not audio"); break; /* dbri interface configured for something else */
#endif      
    }
  return("unknown");
}

static char *sun_in_device_name(int dev)
{
  if (dev == AUDIO_MICROPHONE) return("microphone");
  if (dev == AUDIO_LINE_IN) return("line in");
  if (dev == AUDIO_INTERNAL_CD_IN) return("cd");
  if (dev == (AUDIO_MICROPHONE | AUDIO_LINE_IN)) return("microphone + line in");
  if (dev == (AUDIO_MICROPHONE | AUDIO_LINE_IN | AUDIO_INTERNAL_CD_IN)) return("microphone + line in + cd");
  if (dev == (AUDIO_MICROPHONE | AUDIO_INTERNAL_CD_IN)) return("microphone + cd");
  if (dev == (AUDIO_LINE_IN | AUDIO_INTERNAL_CD_IN)) return("line in + cd");
  return("unknown");
}

static char *sun_out_device_name(int dev)
{
  if (dev == AUDIO_SPEAKER) return("speakers");
  if (dev == AUDIO_LINE_OUT) return("line out");
  if (dev == AUDIO_HEADPHONE) return("headphones");
  if (dev == (AUDIO_SPEAKER | AUDIO_LINE_OUT)) return("speakers + line out");
  if (dev == (AUDIO_SPEAKER | AUDIO_LINE_OUT | AUDIO_HEADPHONE)) return("speakers + line out + headphones");
  if (dev == (AUDIO_SPEAKER | AUDIO_HEADPHONE)) return("speakers + headphones");
  if (dev == (AUDIO_LINE_OUT | AUDIO_HEADPHONE)) return("line out + headphones");
  return("unknown");
}

static char *sun_vol_name = NULL;
static char *sun_volume_name(float vol, int balance, int chans)
{
  if (sun_vol_name == NULL) sun_vol_name = (char *)CALLOC(LABEL_BUFFER_SIZE, sizeof(char));
  if (chans != 2)
    mus_snprintf(sun_vol_name, LABEL_BUFFER_SIZE, "%.3f", vol);
  else 
    {
      mus_snprintf(sun_vol_name, LABEL_BUFFER_SIZE, "%.3f %.3f",
		   vol * (float)(AUDIO_RIGHT_BALANCE - balance) / (float)AUDIO_RIGHT_BALANCE,
		   vol * (float)balance / (float)AUDIO_RIGHT_BALANCE);
    }
  return(sun_vol_name);
}

static void describe_audio_state_1(void) 
{
  struct audio_info info;
#ifndef AUDIO_DEV_AMD
  struct audio_device ad;
#else
  int ad;
#endif
  int audio_fd, err;
  char *dev_name;
  AUDIO_INITINFO(&info);
  if (getenv(AUDIODEV_ENV) != NULL) 
    dev_name = getenv(AUDIODEV_ENV); 
  else dev_name = DAC_NAME;
  audio_fd = open(dev_name, O_RDONLY | O_NONBLOCK, 0);
  if (audio_fd == -1)
    audio_fd = open("/dev/audioctl", O_RDONLY | O_NONBLOCK, 0);
  if (audio_fd == -1) return;
  err = ioctl(audio_fd, AUDIO_GETINFO, &info); 
  if (err == -1) return;
  err = ioctl(audio_fd, AUDIO_GETDEV, &ad); 
  if (err == -1) return;
  pprint(mus_audio_moniker());
  pprint("\n");
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "output: %s\n    srate: %d, vol: %s, chans: %d, format %d-bit %s\n",
	  sun_out_device_name(info.play.port),
	  info.play.sample_rate,
	  sun_volume_name((float)info.play.gain / (float)AUDIO_MAX_GAIN, info.play.balance, 2),
	  info.play.channels,
	  info.play.precision,
	  sun_format_name(info.play.encoding));
  pprint(audio_strbuf);
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "input: %s\n    srate: %d, vol: %s, chans: %d, format %d-bit %s\n",
	  sun_in_device_name(info.record.port),
	  info.record.sample_rate,
	  sun_volume_name((float)info.record.gain / (float)AUDIO_MAX_GAIN, info.record.balance, 2),
	  info.record.channels,
	  info.record.precision,
	  sun_format_name(info.record.encoding));
  pprint(audio_strbuf);
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "input->output vol: %.3f\n", (float)info.monitor_gain / (float)AUDIO_MAX_GAIN);
  pprint(audio_strbuf);
  if (info.play.pause) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "Playback is paused\n"); pprint(audio_strbuf);}
  if (info.record.pause) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "Recording is paused\n"); pprint(audio_strbuf);}
  if (info.output_muted) {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "Output is muted\n"); pprint(audio_strbuf);}
#ifdef AUDIO_HWFEATURE_DUPLEX
  if (info.hw_features == AUDIO_HWFEATURE_DUPLEX) 
    {mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "Simultaneous play and record supported\n"); pprint(audio_strbuf);}
#endif
#if HAVE_SYS_MIXER_H
  {
    int i, num = 0, choice;
    #define LARGE_NUMBER 100
    am_sample_rates_t *sr = NULL;
    for (choice = 0; choice < 2; choice++)
      {
	for (num = 4; num < LARGE_NUMBER; num += 2) 
	  {
	    sr = (am_sample_rates_t *)
	      malloc(AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(num));
	    sr->num_samp_rates = num;
	    sr->type = (choice == 0) ? AUDIO_PLAY : AUDIO_RECORD;
	    err = ioctl(audio_fd, AUDIO_MIXER_GET_SAMPLE_RATES, sr);
	    if ((int)(sr->num_samp_rates) <= num) break;
	    free(sr);
	    sr = NULL;
	  }
	if (sr)
	  {
	    mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s srates:", (choice == 0) ? "play" : "record"); 
	    pprint(audio_strbuf);
	    if (sr->type == MIXER_SR_LIMITS)
	      {
		mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %d to %d", sr->samp_rates[0], sr->samp_rates[sr->num_samp_rates - 1]);
		pprint(audio_strbuf);
	      }
	    else
	      {
		for (i = 0; i < (int)(sr->num_samp_rates); i++)
		  {
		    mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %d", sr->samp_rates[i]);
		    pprint(audio_strbuf);
		  }
	      }
	    pprint("\n");
	  }
	free(sr);
	sr = NULL;
      }
  }
#endif
  mus_audio_close(audio_fd);
}

#endif



/* ------------------------------- WINDOZE ----------------------------------------- */

#if defined(MUS_WINDOZE) && (!(defined(__CYGWIN__)))
#define AUDIO_OK

#include <windows.h>
#include <mmsystem.h>

#define BUFFER_FILLED 1
#define BUFFER_EMPTY 2

#define OUTPUT_LINE 1
#define INPUT_LINE 2

#define SOUND_UNREADY 0
#define SOUND_INITIALIZED 1
#define SOUND_RUNNING 2

static int buffer_size = 1024;
static int db_state[2];
static int sound_state = 0;
static int current_chans = 1;
static int current_datum_size = 2;
static int current_buf = 0;
WAVEHDR wh[2];
HWAVEOUT fd;
HWAVEIN record_fd;
WAVEHDR rec_wh;
static int rec_state = SOUND_UNREADY;

static MMRESULT win_in_err = 0, win_out_err = 0;
static char errstr[128], getstr[128];

static char *win_err_buf = NULL;
static mus_print_handler_t *old_handler;

static void win_mus_print(char *msg)
{
  if ((win_in_err == 0) && (win_out_err == 0))
    (*old_handler)(msg);
  else
    {
      if (win_in_err) 
	waveInGetErrorText(win_in_err, getstr, PRINT_BUFFER_SIZE);
      else waveOutGetErrorText(win_out_err, getstr, PRINT_BUFFER_SIZE);
      mus_snprintf(errstr, PRINT_BUFFER_SIZE, "%s [%s]", msg, getstr);
      (*old_handler)(errstr);
    }
}

static void start_win_print(void)
{
  if (old_handler != win_mus_print)
    old_handler = mus_print_set_handler(win_mus_print);
}

static void end_win_print(void)
{
  if (old_handler == win_mus_print)
    mus_print_set_handler(NULL);
  else mus_print_set_handler(old_handler);
}

#define RETURN_ERROR_EXIT(Error_Type, Ur_Error_Message) \
  do { char *Error_Message; Error_Message = Ur_Error_Message; \
    if (Error_Message) \
      {MUS_STANDARD_ERROR(Error_Type, Error_Message); FREE(Error_Message);} \
    else MUS_STANDARD_ERROR(Error_Type, mus_error_type_to_string(Error_Type)); \
    end_win_print(); \
    return(MUS_ERROR); \
  } while (false)

int mus_audio_systems(void) 
{
  /* this number is available -- see below (user mixer number as in linux)->mixerGetNumDevs */
  return(1);
}
char *mus_audio_system_name(int system) {return("Windoze");}

DWORD CALLBACK next_buffer(HWAVEOUT w, UINT msg, DWORD user_data, DWORD p1, DWORD p2)
{
  if (msg == WOM_DONE)
    {
      db_state[current_buf] = BUFFER_EMPTY;
    }
  return(0);
}

int mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size) 
{
  WAVEFORMATEX wf;
  int dev;
  start_win_print();
  dev = MUS_AUDIO_DEVICE(ur_dev);
  wf.nChannels = chans;
  current_chans = chans;
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.cbSize = 0;
  if (format == MUS_UBYTE) 
    {
      wf.wBitsPerSample = 8;
      current_datum_size = 1;
    }
  else 
    {
      wf.wBitsPerSample = 16;
      current_datum_size = 2;
    }
  wf.nSamplesPerSec = srate;
  wf.nBlockAlign = chans * current_datum_size;
  wf.nAvgBytesPerSec = wf.nBlockAlign * wf.nSamplesPerSec;
  win_out_err = waveOutOpen(&fd, WAVE_MAPPER, &wf, (DWORD)next_buffer, 0, CALLBACK_FUNCTION); /* 0 here = user_data above, other case = WAVE_FORMAT_QUERY */
  if (win_out_err) 
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE,
		      mus_format("can't open %d (%s)",
				 dev, 
				 mus_audio_device_name(dev)));
  waveOutPause(fd);
  if (size <= 0) 
    buffer_size = 1024; 
  else buffer_size = size;
  wh[0].dwBufferLength = buffer_size * current_datum_size;
  wh[0].dwFlags = 0;
  wh[0].dwLoops = 0;
  wh[0].lpData = (char *)CALLOC(wh[0].dwBufferLength, sizeof(char));
  if ((wh[0].lpData) == 0) 
    {
      waveOutClose(fd); 
      RETURN_ERROR_EXIT(MUS_AUDIO_SIZE_NOT_AVAILABLE,
			mus_format("can't allocate buffer size %d for output %d (%s)",
				   buffer_size, dev, 
				   mus_audio_device_name(dev)));
    }
  win_out_err = waveOutPrepareHeader(fd, &(wh[0]), sizeof(WAVEHDR));
  if (win_out_err) 
    {
      FREE(wh[0].lpData); 
      waveOutClose(fd);  
      RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE,
			mus_format("can't setup output 'header' for %d (%s)",
				   dev, 
				   mus_audio_device_name(dev)));
    }
  db_state[0] = BUFFER_EMPTY;
  wh[1].dwBufferLength = buffer_size * current_datum_size;
  wh[1].dwFlags = 0;
  wh[1].dwLoops = 0;
  wh[1].lpData = (char *)CALLOC(wh[0].dwBufferLength, sizeof(char));
  if ((wh[1].lpData) == 0) 
    {
      FREE(wh[0].lpData); 
      waveOutClose(fd); 
      RETURN_ERROR_EXIT(MUS_AUDIO_SIZE_NOT_AVAILABLE,
			mus_format("can't allocate buffer size %d for output %d (%s)",
				   buffer_size, dev,
				   mus_audio_device_name(dev)));
    }
  win_out_err = waveOutPrepareHeader(fd, &(wh[1]), sizeof(WAVEHDR));
  if (win_out_err) 
    {
      waveOutUnprepareHeader(fd, &(wh[0]), sizeof(WAVEHDR)); 
      FREE(wh[0].lpData); 
      FREE(wh[1].lpData); 
      waveOutClose(fd);  
      RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE,
			mus_format("can't setup output 'header' for %d (%s)",
				   dev, 
				   mus_audio_device_name(dev)));
    }
  db_state[1] = BUFFER_EMPTY;
  sound_state = SOUND_INITIALIZED;
  current_buf = 0;
  end_win_print();
  return(OUTPUT_LINE);
}

static MMRESULT fill_buffer(int dbi, char *inbuf, int instart, int bytes)
{
  int i, j;
  win_out_err = 0;
  if (sound_state == SOUND_UNREADY) return(0);
  for (i = instart, j = 0; j < bytes; j++, i++)
    wh[dbi].lpData[j] = inbuf[i];
  wh[dbi].dwBufferLength = bytes;
  db_state[dbi] = BUFFER_FILLED;
  if ((sound_state == SOUND_INITIALIZED) && 
      (dbi == 1))
    {
      sound_state = SOUND_RUNNING;
      win_out_err = waveOutRestart(fd);
    }
  return(win_out_err);
}

static void wait_for_empty_buffer(int buf)
{
  while (db_state[buf] != BUFFER_EMPTY)
    {
      Sleep(1);      /* in millisecs, so even this may be too much if buf = 256 bytes */
    }
}

int mus_audio_write(int line, char *buf, int bytes) 
{
  int lim, leftover, start;
  start_win_print();
  if (line != OUTPUT_LINE) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE,
		      mus_format("write error: line %d != %d?",
				 line, OUTPUT_LINE));
  win_out_err = 0;
  leftover = bytes;
  start = 0;
  if (sound_state == SOUND_UNREADY) 
    {
      end_win_print(); 
      return(MUS_NO_ERROR);
    }
  while (leftover > 0)
    {
      lim = leftover;
      if (lim > buffer_size) lim = buffer_size;
      leftover -= lim;
      wait_for_empty_buffer(current_buf);
      win_out_err = fill_buffer(current_buf, buf, start, lim);
      if (win_out_err) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE,
			  mus_format("write error on %d",
				     line));
      win_out_err = waveOutWrite(fd, &wh[current_buf], sizeof(WAVEHDR));
      if (win_out_err) 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE,
			  mus_format("write error on %d",
				     line));
      start += lim;
      current_buf++;
      if (current_buf > 1) current_buf = 0;
    }
  return(MUS_NO_ERROR);
}

static float unlog(unsigned short val)
{
  /* 1.0 linear is 0xffff, rest is said to be "logarithmic", whatever that really means here */
  if (val == 0) return(0.0);
  return((float)val / 65536.0);
  /* return(pow(2.0, amp) - 1.0); */ /* doc seems to be bogus */
}

#define SRATE_11025_BITS (WAVE_FORMAT_1S16 | WAVE_FORMAT_1S08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_1M08)
#define SRATE_22050_BITS (WAVE_FORMAT_2S16 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2M08)
#define SRATE_44100_BITS (WAVE_FORMAT_4S16 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4M08)
#define SHORT_SAMPLE_BITS (WAVE_FORMAT_1S16 | WAVE_FORMAT_1M16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4S16 | WAVE_FORMAT_4M16)
#define BYTE_SAMPLE_BITS (WAVE_FORMAT_1S08 | WAVE_FORMAT_1M08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4M08)

static char *mixer_status_name(int status)
{
  switch (status)
    {
    case MIXERLINE_LINEF_ACTIVE: return(", (active)"); break;
    case MIXERLINE_LINEF_DISCONNECTED: return(", (disconnected)"); break;
    case MIXERLINE_LINEF_SOURCE: return(", (source)"); break;
    default: return(""); break;
    }
}

static char *mixer_target_name(int type)
{
  switch (type)
    {
    case MIXERLINE_TARGETTYPE_UNDEFINED: return("undefined"); break;
    case MIXERLINE_TARGETTYPE_WAVEOUT: return("output"); break;
    case MIXERLINE_TARGETTYPE_WAVEIN: return("input"); break;
    case MIXERLINE_TARGETTYPE_MIDIOUT: return("midi output"); break;
    case MIXERLINE_TARGETTYPE_MIDIIN: return("midi input"); break;
    case MIXERLINE_TARGETTYPE_AUX: return("aux"); break;
    default: return(""); break;
    }
}

static char *mixer_component_name(int type)
{
  switch (type)
    {
    case MIXERLINE_COMPONENTTYPE_DST_UNDEFINED: return("undefined"); break;
    case MIXERLINE_COMPONENTTYPE_DST_DIGITAL: return("digital"); break;
    case MIXERLINE_COMPONENTTYPE_DST_LINE: return("line"); break;
    case MIXERLINE_COMPONENTTYPE_DST_MONITOR: return("monitor"); break;
    case MIXERLINE_COMPONENTTYPE_DST_SPEAKERS: return("speakers"); break;
    case MIXERLINE_COMPONENTTYPE_DST_HEADPHONES: return("headphones"); break;
    case MIXERLINE_COMPONENTTYPE_DST_TELEPHONE: return("telephone"); break;
    case MIXERLINE_COMPONENTTYPE_DST_WAVEIN: return("wave in"); break;
    case MIXERLINE_COMPONENTTYPE_DST_VOICEIN: return("voice in"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED: return("undefined"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_DIGITAL: return("digital"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_LINE: return("line"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE: return("mic"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER: return("synth"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC: return("CD"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_TELEPHONE: return("telephone"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER: return("speaker"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT: return("wave out"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY: return("aux"); break;
    case MIXERLINE_COMPONENTTYPE_SRC_ANALOG: return("analog"); break;
    default: return(""); break;
    }
}

#define MAX_DESCRIBE_CHANS 8
#define MAX_DESCRIBE_CONTROLS 16
/* these actually need to be big enough to handle whatever comes along, since we can't read partial states */
/*   or they need to be expanded as necessary */

char *mus_audio_moniker(void) {return("MS audio");} /* version number of some sort? */

static void describe_audio_state_1(void) 
{
  int devs, dev, srate, chans, format, need_comma, maker;
  MMRESULT err;
  unsigned long val, rate, pitch, version;
  WAVEOUTCAPS wocaps;
  WAVEINCAPS wicaps;
  AUXCAPS wacaps;
  HWAVEOUT hd;
  WAVEFORMATEX pwfx;
#ifdef MIXERR_BASE
  MIXERCAPS wmcaps;
  MIXERLINE mixline;
  MIXERLINECONTROLS linecontrols;
  MIXERCONTROL mc[MAX_DESCRIBE_CONTROLS];
  MIXERCONTROLDETAILS controldetails;
  MIXERCONTROLDETAILS_LISTTEXT clist[MAX_DESCRIBE_CHANS];
  MIXERCONTROLDETAILS_BOOLEAN cbool[MAX_DESCRIBE_CHANS];
  MIXERCONTROLDETAILS_UNSIGNED cline[MAX_DESCRIBE_CHANS];
  MIXERCONTROLDETAILS_SIGNED csign[MAX_DESCRIBE_CHANS];
  HMIXER mfd;
  int control, controls, dest, dests, source, happy, dest_time, chan, mina, maxa, ctype;
#endif
  need_comma = 1;
  chans = 1;
  devs = waveOutGetNumDevs();
  if (devs > 0)
    {
      pprint("Output:\n");
      for (dev = 0; dev < devs; dev++)
        {
          err = waveOutGetDevCaps(dev, &wocaps, sizeof(wocaps));
          if (!err)
            {
              version = wocaps.vDriverVersion;
              maker = wocaps.wMid;
              mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %s: version %d.%d\n",
			   wocaps.szPname, version >> 8, version & 0xff);
              pprint(audio_strbuf);
              if (wocaps.wChannels == 2) {chans = 2; pprint("    stereo");} else {chans = 1; pprint("    mono");}
              if (wocaps.dwFormats & SRATE_11025_BITS)  {srate = 11025; if (need_comma) pprint(", "); pprint(" 11025"); need_comma = 1;}
              if (wocaps.dwFormats & SRATE_22050_BITS)  {srate = 22050; if (need_comma) pprint(", "); pprint(" 22050"); need_comma = 1;}
              if (wocaps.dwFormats & SRATE_44100_BITS)  {srate = 44100; if (need_comma) pprint(", "); pprint(" 44100"); need_comma = 1;}
              if (wocaps.dwFormats & BYTE_SAMPLE_BITS)  {format = 8; if (need_comma) pprint(", "); pprint(" unsigned byte"); need_comma = 1;}
              if (wocaps.dwFormats & SHORT_SAMPLE_BITS) {format = 16; if (need_comma) pprint(", "); pprint(" little-endian short"); need_comma = 1;}
              if (need_comma) pprint("\n");
              need_comma = 0;
              pwfx.wFormatTag = WAVE_FORMAT_PCM;
              pwfx.nChannels = chans;
              pwfx.nSamplesPerSec = srate;
              pwfx.nAvgBytesPerSec = srate;
              pwfx.nBlockAlign = 1;
              pwfx.wBitsPerSample = format;

              err = waveOutOpen(&hd, dev, &pwfx, 0, 0, WAVE_FORMAT_QUERY);

              if (wocaps.dwSupport & WAVECAPS_VOLUME)
                {
                  err = waveOutGetVolume(hd, &val);
                  if (!err)
                    {
                      if (wocaps.dwSupport & WAVECAPS_LRVOLUME)
                        mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
				     "  vol: %.3f %.3f", 
				     unlog((unsigned short)(val >> 16)), 
				     unlog((unsigned short)(val & 0xffff)));
                      else mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
					"  vol: %.3f", 
					unlog((unsigned short)(val & 0xffff)));
                      pprint(audio_strbuf);
                      need_comma = 1;
                    }
                }
              if (!err)
                {
                  /* this is just to get the hd data for subsequent info */
                  if (wocaps.dwSupport & WAVECAPS_PLAYBACKRATE)
                    {
                      err = waveOutGetPlaybackRate(hd, &rate);
                      if (!err)
                        {
                          mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
				       "%s playback rate: %.3f", 
				       (need_comma ? ", " : ""), 
				       (float)rate / 65536.0);
                          pprint(audio_strbuf);
                          need_comma = 1;
                        }
                    }
                  if (wocaps.dwSupport & WAVECAPS_PITCH)
                    {
                      err = waveOutGetPitch(hd, &pitch);
                      if (!err)
                        {
                          mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
				       "%s pitch: %.3f", 
				       (need_comma ? ", " : ""), 
				       (float)pitch / 65536.0);
                          pprint(audio_strbuf);
                          need_comma = 1;
                        }
                    }
                  waveOutClose(hd);
                }
              if (need_comma) {need_comma = 0; pprint("\n");}
            }
        }
    }
  devs = waveInGetNumDevs();
  if (devs > 0)
    {
      pprint("Input:\n");
      for (dev = 0; dev < devs; dev++)
        {
          err = waveInGetDevCaps(dev, &wicaps, sizeof(wicaps));
          if (!err)
            {
              mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %s", wicaps.szPname);
              pprint(audio_strbuf);
              if ((wicaps.wMid != maker) || (version != wicaps.vDriverVersion))
                {
                  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, ": version %d.%d\n", 
			       (wicaps.vDriverVersion >> 8), 
			       wicaps.vDriverVersion & 0xff);
                  pprint(audio_strbuf);
                }
              else pprint("\n");
              if (wicaps.wChannels == 2) pprint("    stereo"); else pprint("    mono");
              if (wicaps.dwFormats & SRATE_11025_BITS)  {pprint(", 11025"); need_comma = 1;}
              if (wicaps.dwFormats & SRATE_22050_BITS)  {if (need_comma) pprint(", "); pprint(" 22050"); need_comma = 1;}
              if (wicaps.dwFormats & SRATE_44100_BITS)  {if (need_comma) pprint(", "); pprint(" 44100"); need_comma = 1;}
              if (wicaps.dwFormats & BYTE_SAMPLE_BITS)  {if (need_comma) pprint(", "); pprint(" unsigned byte"); need_comma = 1;}
              if (wicaps.dwFormats & SHORT_SAMPLE_BITS) {if (need_comma) pprint(", "); pprint(" little-endian short");}
              pprint("\n");
            }
        }
    }
  devs = auxGetNumDevs();
  if (devs > 0)
    {
      pprint("Auxiliary:\n");
      for (dev = 0; dev < devs; dev++)
        {
          err = auxGetDevCaps(dev, &wacaps, sizeof(wacaps));
          if (!err)
            {
              mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %s", wacaps.szPname);
              pprint(audio_strbuf);
              if ((wacaps.wMid != maker) || (version != wacaps.vDriverVersion))
                mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, ": version %d.%d%s",
                        (wacaps.vDriverVersion >> 8), wacaps.vDriverVersion & 0xff,
                        (wacaps.wTechnology & AUXCAPS_CDAUDIO) ? " (CD)" : "");
              else mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s\n", (wacaps.wTechnology & AUXCAPS_CDAUDIO) ? " (CD)" : "");
              pprint(audio_strbuf);
              if (wacaps.dwSupport & AUXCAPS_VOLUME)
                {
                  err = auxGetVolume(dev, &val);
                  if (!err)
                    {
                      if (wacaps.dwSupport & AUXCAPS_LRVOLUME)
                        mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
				     "  vol: %.3f %.3f\n", 
				     unlog((unsigned short)(val >> 16)), 
				     unlog((unsigned short)(val & 0xffff)));
                      else mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
					"  vol: %.3f\n", 
					unlog((unsigned short)(val & 0xffff)));
                      pprint(audio_strbuf);
                    }
                }
            }
        }
    }
#ifdef MIXERR_BASE
  devs = mixerGetNumDevs();
  if (devs > 0)
    {
      pprint("Mixer:\n");
      for (dev = 0; dev < devs; dev++)
        {
          err = mixerGetDevCaps(dev, &wmcaps, sizeof(wmcaps));
          if (!err)
            {
              mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  %s", wmcaps.szPname);
              pprint(audio_strbuf);
              if ((wmcaps.wMid != maker) || (version != wmcaps.vDriverVersion))
                {
                  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE,
			       ": version %d.%d\n", 
			       (wmcaps.vDriverVersion >> 8), 
			       wmcaps.vDriverVersion & 0xff);
                  pprint(audio_strbuf);
                }
              else pprint("\n");
              dests = wmcaps.cDestinations;
              
              err = mixerOpen(&mfd, dev, 0, 0, CALLBACK_NULL);
              if (!err)
                {
                  dest = 0;
                  source = 0;
                  dest_time = 1;
                  happy = 1;
                  while (happy)
                    {
                      if (dest_time)
                        {
                          mixline.dwDestination = dest;
                          mixline.cbStruct = sizeof(MIXERLINE);
                          err = mixerGetLineInfo((HMIXEROBJ)mfd, &mixline, MIXER_GETLINEINFOF_DESTINATION);
                        }
                      else
                        {
                          mixline.dwSource = source;
                          mixline.cbStruct = sizeof(MIXERLINE);
                          err = mixerGetLineInfo((HMIXEROBJ)mfd, &mixline, MIXER_GETLINEINFOF_SOURCE);
                        }
                      if (!err)
                        {
                          if ((source == 0) && (!dest_time)) pprint("  Sources:\n");
                          if ((dest == 0) && (dest_time)) pprint("  Destinations:\n");
                          mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "    %s: %s (%s), %d chan%s",
                                  mixline.szName,
                                  mixer_component_name(mixline.dwComponentType),
                                  mixer_target_name(mixline.Target.dwType),
                                  
                                  mixline.cChannels, ((mixline.cChannels != 1) ? "s" : ""));
                          pprint(audio_strbuf);
                          if (mixline.cConnections > 0)
                                {
                                    mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, ", %d connection%s",
                                    mixline.cConnections, ((mixline.cConnections != 1) ? "s" : ""));
                                    pprint(audio_strbuf);
                                }
                          if (dest_time) 
                            {
                              mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s\n", mixer_status_name(mixline.fdwLine));
                              pprint(audio_strbuf);
                            }
                          else pprint("\n");
                          if (mixline.cControls > 0)
                            {
                              linecontrols.cbStruct = sizeof(MIXERLINECONTROLS);
                              linecontrols.dwLineID = mixline.dwLineID;
                              linecontrols.dwControlID = MIXER_GETLINECONTROLSF_ONEBYID;
                              if (mixline.cControls > MAX_DESCRIBE_CONTROLS)
                                linecontrols.cControls = MAX_DESCRIBE_CONTROLS;
                              else linecontrols.cControls = mixline.cControls;
                              linecontrols.pamxctrl = mc;
                              linecontrols.cbmxctrl = sizeof(MIXERCONTROL);
                              err = mixerGetLineControls((HMIXEROBJ)mfd, &linecontrols, MIXER_GETLINECONTROLSF_ALL);
                              if (!err)
                                {
                                  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, 
					       "      %d control%s:\n", 
					       linecontrols.cControls, 
					       (linecontrols.cControls != 1) ? "s" : "");
                                  pprint(audio_strbuf);
                                  controls = linecontrols.cControls;
                                  if (controls > MAX_DESCRIBE_CONTROLS) controls = MAX_DESCRIBE_CONTROLS;
                                  for (control = 0; control < controls; control++)
                                    {

                                       mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "        %s", mc[control].szName);
                                       pprint(audio_strbuf);
                                       controldetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
                                       controldetails.dwControlID = mc[control].dwControlID;

				       ctype = (mc[control].dwControlType);
				       if ((ctype == MIXERCONTROL_CONTROLTYPE_EQUALIZER) ||
					   (ctype == MIXERCONTROL_CONTROLTYPE_MUX) ||
					   (ctype == MIXERCONTROL_CONTROLTYPE_MIXER) ||
					   (ctype == MIXERCONTROL_CONTROLTYPE_SINGLESELECT) ||
					   (ctype == MIXERCONTROL_CONTROLTYPE_MULTIPLESELECT))
					 {
					   controldetails.cChannels = 1;
                                           controldetails.cMultipleItems = mc[control].cMultipleItems;
                                           controldetails.cbDetails = sizeof(MIXERCONTROLDETAILS_LISTTEXT);
                                           controldetails.paDetails = clist;
					   err = mixerGetControlDetails((HMIXEROBJ)mfd, &controldetails, MIXER_GETCONTROLDETAILSF_LISTTEXT);
					   if (!err) 
					     {
					       for (chan = 0; chan < mixline.cChannels; chan++) 
						 {
						   mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " [%s]", clist[chan].szName);
						   pprint(audio_strbuf);
						 }
					     }
					 }
                                       if (mixline.cChannels > MAX_DESCRIBE_CHANS)
                                         controldetails.cChannels = MAX_DESCRIBE_CHANS;
                                       else controldetails.cChannels = mixline.cChannels;
                                       controldetails.cMultipleItems = 0;
                                       err = 0;
                                       switch (mc[control].dwControlType & MIXERCONTROL_CT_UNITS_MASK)
                                         {
                                         case MIXERCONTROL_CT_UNITS_BOOLEAN:
                                           controldetails.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
                                           controldetails.paDetails = cbool;
                                           break;
                                         case MIXERCONTROL_CT_UNITS_SIGNED: case MIXERCONTROL_CT_UNITS_DECIBELS:
                                           controldetails.cbDetails = sizeof(MIXERCONTROLDETAILS_SIGNED);
                                           controldetails.paDetails = csign;
                                           break;
                                         case MIXERCONTROL_CT_UNITS_UNSIGNED: case MIXERCONTROL_CT_UNITS_PERCENT:
                                           controldetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                                           controldetails.paDetails = cline;
                                           break;
                                         default: err = 1; break;
                                         }
                                       if (err) 
                                         pprint("\n");
                                       else
                                         {
                                           err = mixerGetControlDetails((HMIXEROBJ)mfd, &controldetails, MIXER_GETCONTROLDETAILSF_VALUE);
                                           if (!err)
                                             {
                                               chans = controldetails.cChannels;
                                               if (chans > MAX_DESCRIBE_CHANS) chans = MAX_DESCRIBE_CHANS;
                                               switch (mc[control].dwControlType & MIXERCONTROL_CT_UNITS_MASK)
                                                 {
                                                 case MIXERCONTROL_CT_UNITS_BOOLEAN:
                                                   for (chan = 0; chan < chans; chan++)
                                                     {
                                                       mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %s", (cbool[chan].fValue) ? " on" : " off");
                                                       pprint(audio_strbuf);
                                                     }
                                                   break;
                                                 case MIXERCONTROL_CT_UNITS_SIGNED: case MIXERCONTROL_CT_UNITS_DECIBELS:
                                                   mina = mc[control].Bounds.lMinimum;
                                                   maxa = mc[control].Bounds.lMaximum;
                                                   if (maxa > mina)
                                                     {
                                                       for (chan = 0; chan < chans; chan++)
                                                         {
                                                           mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.3f", 
									(float)(csign[chan].lValue - mina) / (float)(maxa - mina));
                                                           pprint(audio_strbuf);
                                                         }
                                                     }
                                                   break;
                                                 case MIXERCONTROL_CT_UNITS_UNSIGNED: case MIXERCONTROL_CT_UNITS_PERCENT:
                                                   mina = mc[control].Bounds.dwMinimum;
                                                   maxa = mc[control].Bounds.dwMaximum;
                                                   if (maxa > mina)
                                                     {
                                                       for (chan = 0; chan < chans; chan++)
                                                         {
                                                           mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %.3f", 
									(float)(cline[chan].dwValue - mina) / (float)(maxa - mina));
                                                           pprint(audio_strbuf);
                                                         }
                                                     }
                                                   break;
                                                 default: break;
                                                 }
                                               pprint("\n");
                                             }
                                           else pprint("\n");
                                         }
                                    }
                                }
                            }
                        }
                      else if (!dest_time) happy = 0;
                      if (dest_time) dest++; else source++;
                      if (dest == dests) dest_time = 0;
                    }
                }
              mixerClose(mfd);
            }
        }
    }
#endif
}

int mus_audio_initialize(void) 
{
  return(MUS_NO_ERROR);
}

int mus_audio_close(int line) 
{
  int i;
  win_out_err = 0; 
  win_in_err = 0;
  if (line == OUTPUT_LINE)
    {
      /* fill with a few zeros, wait for empty flag */
      if (sound_state != SOUND_UNREADY)
        {
          wait_for_empty_buffer(current_buf);
          for (i = 0; i < 128; i++) wh[current_buf].lpData[i] = 0;
          wait_for_empty_buffer(current_buf);
          win_out_err = waveOutClose(fd);
	  i = 0;
          while (win_out_err == WAVERR_STILLPLAYING)
            {
	      Sleep(1);
              win_out_err = waveOutClose(fd);
	      i++;
	      if (i > 1024) break;
            }
          db_state[0] = BUFFER_EMPTY;
          db_state[1] = BUFFER_EMPTY;
          sound_state = SOUND_UNREADY;
          waveOutUnprepareHeader(fd, &(wh[0]), sizeof(WAVEHDR));
          waveOutUnprepareHeader(fd, &(wh[1]), sizeof(WAVEHDR));
          FREE(wh[0].lpData);
          FREE(wh[1].lpData);
          if (win_out_err) 
	    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_CLOSE,
			      mus_format("close failed on %d",
					 line));
        }
    }
  else 
    {
      if (line == INPUT_LINE)
        {
          if (rec_state != SOUND_UNREADY)
            {
              waveInReset(record_fd);
              waveInClose(record_fd);
              waveInUnprepareHeader(record_fd, &rec_wh, sizeof(WAVEHDR));
              if (rec_wh.lpData) 
		{
		  FREE(rec_wh.lpData);
		  rec_wh.lpData = NULL;
		}
              rec_state = SOUND_UNREADY;
            }
        }
      else 
	RETURN_ERROR_EXIT(MUS_AUDIO_CANT_CLOSE,
			  mus_format("can't close unrecognized line %d",
				     line));
    }
  return(MUS_NO_ERROR);
}

  /*
   * waveInAddBuffer sends buffer to get data
   * MM_WIM_DATA lParam->WAVEHDR dwBytesRecorded =>how much data actually in buffer
   */

static int current_record_chans = 0, current_record_datum_size = 0;

DWORD CALLBACK next_input_buffer(HWAVEIN w, UINT msg, DWORD user_data, DWORD p1, DWORD p2)
{
  if (msg == WIM_DATA)
    {
      /* grab data */
      /* p1->dwBytesRecorded */
    }
  return(0);
}

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int size) 
{
  WAVEFORMATEX wf;
  int dev;
  win_in_err = 0;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  wf.nChannels = chans;
  current_record_chans = chans;

  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.cbSize = 0;
  if (format == MUS_UBYTE) 
    {
      wf.wBitsPerSample = 8;
      current_record_datum_size = 1;
    }
  else 
    {
      wf.wBitsPerSample = 16;
      current_record_datum_size = 2;
    }
  wf.nSamplesPerSec = srate;
  wf.nBlockAlign = chans * current_datum_size;
  wf.nAvgBytesPerSec = wf.nBlockAlign * wf.nSamplesPerSec;

  rec_wh.dwBufferLength = size * current_record_datum_size;
  rec_wh.dwFlags = 0;
  rec_wh.dwLoops = 0;
  rec_wh.lpData = (char *)CALLOC(rec_wh.dwBufferLength, sizeof(char));
  if ((rec_wh.lpData) == 0) 
    RETURN_ERROR_EXIT(MUS_AUDIO_SIZE_NOT_AVAILABLE,
		      mus_format("can't allocated %d bytes for input buffer of %d (%s)",
				 size, dev, mus_audio_device_name(dev)));
  win_in_err = waveInOpen(&record_fd, WAVE_MAPPER, &wf, (DWORD)next_input_buffer, 0, CALLBACK_FUNCTION);
  if (win_in_err) 
    {
      FREE(rec_wh.lpData);
      RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE,
			mus_format("can't open input device %d (%s)",
				   dev, mus_audio_device_name(dev)));
    }
  win_in_err = waveInPrepareHeader(record_fd, &(rec_wh), sizeof(WAVEHDR));
  if (win_in_err) 
    {
      FREE(rec_wh.lpData);
      waveInClose(record_fd);
      RETURN_ERROR_EXIT(MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE,
			mus_format("can't prepare input 'header' for %d (%s)",
				   dev, mus_audio_device_name(dev))); 
    }
  return(MUS_NO_ERROR);
}

int mus_audio_read(int line, char *buf, int bytes) 
{
  win_in_err = 0;
  return(MUS_ERROR);
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val) 
{
  int dev, sys;
  unsigned long lval;
  MMRESULT err;
  sys = MUS_AUDIO_SYSTEM(ur_dev);
  dev = MUS_AUDIO_DEVICE(ur_dev);
  if (field == MUS_AUDIO_AMP)
    {
      err = auxGetVolume(sys, &lval);
      if (!err)
	{
	  if (chan == 0)
	    val[0] = unlog((unsigned short)(lval >> 16));
	  else val[0] = unlog((unsigned short)(lval & 0xffff));
	  return(MUS_NO_ERROR);
	}
    }
  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ,
		    mus_format("can't read device %d (%s) field %s",
			       dev, mus_audio_device_name(dev),
			       mus_audio_device_name(field)));
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val) 
{
  int dev, sys;
  unsigned long lval;
  MMRESULT err;
  sys = MUS_AUDIO_SYSTEM(ur_dev);
  dev = MUS_AUDIO_DEVICE(ur_dev);
  if (field == MUS_AUDIO_AMP)
    {
      err = auxGetVolume(sys, &lval);
      if (!err)
	{
	  if (chan == 0)
	    lval = (unsigned long)((lval & 0xffff) | (((unsigned short)(val[0] * 65535)) << 16));
	  else lval = (unsigned long)((lval & 0xffff0000) | ((unsigned short)(val[0] * 65535)));
	  err = auxSetVolume(sys, lval);
	  if (err) return(MUS_NO_ERROR);
	}
    }
  RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE,
		    mus_format("can't set device %d (%s) field %s",
			       dev, mus_audio_device_name(dev),
			       mus_audio_device_name(field)));
}

#endif



/* ------------------------------- OSX ----------------------------------------- */

/* this code based primarily on the CoreAudio headers and portaudio pa_mac_core.c,
 *   and to a much lesser extent, coreaudio.pdf and the HAL/Daisy examples.
 */

#ifdef MUS_MAC_OSX
#define AUDIO_OK 1

/*
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
*/
/* ./System/Library/Frameworks/CoreAudio.framework/Headers/CoreAudio.h */

static char* osx_error(OSStatus err) 
{
  if (err == noErr) return("no error");
  switch (err) 
    {
    case kAudioHardwareNoError:               return("no error");                         break;
    case kAudioHardwareUnspecifiedError:      return("unspecified audio hardware error"); break;
    case kAudioHardwareNotRunningError:       return("audio hardware not running");       break;
    case kAudioHardwareUnknownPropertyError:  return("unknown property");                 break;
    case kAudioHardwareBadPropertySizeError:  return("bad property");                     break;
    case kAudioHardwareBadDeviceError:        return("bad device");                       break;
    case kAudioHardwareBadStreamError:        return("bad stream");                       break;
    case kAudioHardwareIllegalOperationError: return("illegal operation");                break;
    case kAudioDeviceUnsupportedFormatError:  return("unsupported format");               break;
    case kAudioDevicePermissionsError:        return("device permissions error");         break;
    }
  return("unknown error");
}

char *device_name(AudioDeviceID deviceID, int input_case)
{
  OSStatus err = noErr;
  UInt32 size = 0, msize = 0, trans = 0, trans_size = 0;
  char *name = NULL, *mfg = NULL, *full_name = NULL;
  err =  AudioDeviceGetPropertyInfo(deviceID, 0, false, kAudioDevicePropertyDeviceName, &size, NULL);
  if (err == noErr) err =  AudioDeviceGetPropertyInfo(deviceID, 0, false, kAudioDevicePropertyDeviceManufacturer, &msize, NULL);
  if (err == noErr)
    {
      name = (char *)MALLOC(size + 2);
      err = AudioDeviceGetProperty(deviceID, 0, input_case, kAudioDevicePropertyDeviceName, &size, name);
      mfg = (char *)MALLOC(msize + 2);
      err = AudioDeviceGetProperty(deviceID, 0, input_case, kAudioDevicePropertyDeviceManufacturer, &msize, mfg);
      full_name = (char *)MALLOC(size + msize + 4);
#if HAVE_KAUDIODEVICEPROPERTYTRANSPORTTYPE
      trans_size = sizeof(UInt32);
      err = AudioDeviceGetProperty(deviceID, 0, input_case, kAudioDevicePropertyTransportType, &trans_size, &trans);
      if (err != noErr) 
#endif
	trans = 0;
      if (trans == 0)
	mus_snprintf(full_name, size + msize + 4, "\n  %s: %s", mfg, name);
      else mus_snprintf(full_name, size + msize + 4, "\n  %s: %s ('%c%c%c%c')", 
			mfg, name,
			(char)((trans >> 24) & 0xff), (char)((trans >> 16) & 0xff), (char)((trans >> 8) & 0xff), (char)(trans & 0xff));
      FREE(name);
      FREE(mfg);
    }
  return(full_name);
}	

static int max_chans_via_stream_configuration(AudioDeviceID device, bool input_case)
{
  /* apparently MOTU 828 has to be different (this code from portaudio) */
  UInt32 size = 0;
  Boolean writable;
  OSStatus err = noErr;
  err = AudioDeviceGetPropertyInfo(device, 0, input_case, kAudioDevicePropertyStreamConfiguration, &size, &writable);
  if (err == noErr)
    {
      AudioBufferList *list;
      list = (AudioBufferList *)malloc(size);
      err = AudioDeviceGetProperty(device, 0, input_case, kAudioDevicePropertyStreamConfiguration, &size, list);
      if (err == noErr)
	{
	  int chans = 0, i;
	  for (i = 0; i < list->mNumberBuffers; i++)
	    chans += list->mBuffers[i].mNumberChannels;
	  free(list);
	  return(chans);
	}
    }
  return(-1);
}

static void describe_audio_state_1(void) 
{
  OSStatus err = noErr;
  UInt32 num_devices = 0, msize = 0, size = 0, buffer_size = 0, mute = 0, alive = 0;
  Float32 vol;
  int i, j, k;
  pid_t hogger = 0;
  AudioDeviceID *devices = NULL;
  AudioDeviceID device, default_output, default_input;
  AudioStreamBasicDescription desc;
  AudioStreamBasicDescription *descs = NULL;
  int formats = 0, m;
  bool input_case = false;
  err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &msize, NULL);
  if (err != noErr) 
    {
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "get property info error: %s\n", osx_error(err));
      pprint(audio_strbuf);
      return;
    }
  num_devices = msize / sizeof(AudioDeviceID);
  if (num_devices <= 0) 
    {
      pprint("no audio devices found");
      return;
    }
  devices = (AudioDeviceID *)MALLOC(msize);
  size = sizeof(AudioDeviceID);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &size, &default_input);
  if (err != noErr) default_input = 55555555; /* unsigned int -- I want some value that won't happen! */
  size = sizeof(AudioDeviceID);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &default_output);
  if (err != noErr) default_output = 55555555;
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &msize, (void *)devices);	
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "found %d audio device%s", 
	       (int)num_devices, (num_devices != 1) ? "s" : "");
  pprint(audio_strbuf);
  for (m = 0; m < 2; m++)
    {
      for (i = 0; i < num_devices; i++)
	{
	  device = devices[i];
	  pprint(device_name(device, input_case));
	  if (input_case)
	    {
	      if (device == default_input) 
		pprint(" (default input)"); 
	      else pprint(" (input)");
	    }
	  else
	    {
	      if (device == default_output) 
		pprint(" (default output)"); 
	      else pprint(" (output)");
	    }
	  size = sizeof(pid_t);
	  err = AudioDeviceGetProperty(device, 0, input_case, kAudioDevicePropertyHogMode, &size, &hogger);
	  if ((err == noErr) && (hogger >= 0))
	    {
	      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " currently owned (exclusively) by process %d", (int)hogger);
	      pprint(audio_strbuf); 
	    }
	  size = sizeof(UInt32);
	  err = AudioDeviceGetProperty(device, 0, input_case, kAudioDevicePropertyDeviceIsAlive, &size, &alive);
	  if ((err == noErr) && (alive == 0))
	    pprint(" disconnected?");
	  size = sizeof(UInt32);
	  err = AudioDeviceGetProperty(device, 0, input_case, kAudioDevicePropertyBufferSize, &size, &buffer_size);
	  if (err != noErr) buffer_size = 0;
	  size = sizeof(AudioStreamBasicDescription);
	  err = AudioDeviceGetProperty(device, 0, input_case, kAudioDevicePropertyStreamFormat, &size, &desc);
	  if (err == noErr) 
	    {
	      int config_chans;
	      unsigned int trans;
	      trans = (unsigned int)(desc.mFormatID);
	      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "\n    srate: %d, chans: %d",
			   (int)(desc.mSampleRate), 
			   (int)(desc.mChannelsPerFrame));
	      pprint(audio_strbuf);
	      config_chans = max_chans_via_stream_configuration(device, input_case);
	      if ((config_chans > 0) && (config_chans != (int)(desc.mChannelsPerFrame)))
		{
		  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " (or %d?)", config_chans);
		  pprint(audio_strbuf);
		}
	      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, ", bits/sample: %d, format: %c%c%c%c",
			   (int)(desc.mBitsPerChannel),
			   (trans >> 24) & 0xff, (trans >> 16) & 0xff, (trans >> 8) & 0xff, trans & 0xff);
	      pprint(audio_strbuf);
	      if (buffer_size > 0)
		{
		  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, ", buf: %d", (int)buffer_size);
		  pprint(audio_strbuf);
		}
	      if ((int)(desc.mFormatFlags) != 0) /* assuming "PCM" here */
		{
		  int flags;
		  flags = ((int)(desc.mFormatFlags));
		  pprint("\n    flags: ");
		  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s%s%s%s%s%s",
			       (flags & kLinearPCMFormatFlagIsFloat) ? "float " : "",
			       (flags & kLinearPCMFormatFlagIsBigEndian) ? "big-endian " : "",
			       (flags & kLinearPCMFormatFlagIsSignedInteger) ? "signed-int " : "",
			       (flags & kLinearPCMFormatFlagIsPacked) ? "packed " : "",
			       (flags & kLinearPCMFormatFlagIsAlignedHigh) ? "aligned-high " : "",
#if HAVE_KLINEARPCMFORMATFLAGISNONINTERLEAVED
			       (flags & kLinearPCMFormatFlagIsNonInterleaved) ? "non-interleaved " : ""
#else
		               ""
#endif
			       );
		  pprint(audio_strbuf);
		}

	      if ((int)(desc.mChannelsPerFrame) > 0)
		{
		  pprint("\n    vols:");
		  for (j = 0; j <= (int)(desc.mChannelsPerFrame); j++)
		    {
		      size = sizeof(Float32);
		      err = AudioDeviceGetProperty(device, j, input_case, kAudioDevicePropertyVolumeScalar, &size, &vol);
		      if (err == noErr) 
			{
			  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " %s%.3f", 
				       (j == 0) ? "master: " : "", 
				       vol);
			  pprint(audio_strbuf); 
			}
		      
		      if (j > 0)
			{
			  size = sizeof(UInt32);
			  err = AudioDeviceGetProperty(device, j, input_case, kAudioDevicePropertyMute, &size, &mute);
			  if ((err == noErr) && (mute == 1))
			    {
			      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, " (muted)");
			      pprint(audio_strbuf); 
			    }
			}
		    }
		}
	    }
	  size = 0;
	  err = AudioDeviceGetPropertyInfo(device, 0, input_case, kAudioDevicePropertyStreamFormats, &size, NULL);
	  formats = size / sizeof(AudioStreamBasicDescription);
	  if (formats > 1)
	    {
	      descs = (AudioStreamBasicDescription *)CALLOC(formats, sizeof(AudioStreamBasicDescription));
	      size = formats * sizeof(AudioStreamBasicDescription);
	      err = AudioDeviceGetProperty(device, 0, input_case, kAudioDevicePropertyStreamFormats, &size, descs);
	      if (err == noErr) 
		{
		  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "\n    This device supports %d formats: ", formats); 
		  pprint(audio_strbuf);
		  for (k = 0; k < formats; k++)
		    {
		      unsigned int trans;
		      trans = (unsigned int)(descs[k].mFormatID);
		      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "\n      srate: %d, chans: %d, bits/sample: %d, format: %c%c%c%c",
				   (int)(descs[k].mSampleRate), 
				   (int)(descs[k].mChannelsPerFrame), 
				   (int)(descs[k].mBitsPerChannel),
				   (trans >> 24) & 0xff, (trans >> 16) & 0xff, (trans >> 8) & 0xff, trans & 0xff);					 
		      pprint(audio_strbuf);
		    }
		}
	      FREE(descs);
	    }
	  pprint("\n");
	}
      input_case = true;
    }
  if (devices) FREE(devices);
}

#define MAX_BUFS 4
static char **bufs = NULL;
static int in_buf = 0, out_buf = 0;

static OSStatus writer(AudioDeviceID inDevice, 
		       const AudioTimeStamp *inNow, 
		       const AudioBufferList *InputData, const AudioTimeStamp *InputTime, 
		       AudioBufferList *OutputData, const AudioTimeStamp *OutputTime, 
		       void *appGlobals)
{
  AudioBuffer abuf;
  char *aplbuf, *sndbuf;
  abuf = OutputData->mBuffers[0];
  aplbuf = (char *)(abuf.mData);
  sndbuf = bufs[out_buf];
  memmove((void *)aplbuf, (void *)sndbuf, abuf.mDataByteSize);
  out_buf++;
  if (out_buf >= MAX_BUFS) out_buf = 0;
  return(noErr);
}

static OSStatus reader(AudioDeviceID inDevice, 
		       const AudioTimeStamp *inNow, 
		       const AudioBufferList *InputData, const AudioTimeStamp *InputTime, 
		       AudioBufferList *OutputData, const AudioTimeStamp *OutputTime, 
		       void *appGlobals)
{
  AudioBuffer abuf;
  char *aplbuf, *sndbuf;
  abuf = InputData->mBuffers[0];
  aplbuf = (char *)(abuf.mData);
  sndbuf = bufs[out_buf];
  memmove((void *)sndbuf, (void *)aplbuf, abuf.mDataByteSize);
  out_buf++;
  if (out_buf >= MAX_BUFS) out_buf = 0;
  return(noErr);
}


static AudioDeviceID device = kAudioDeviceUnknown;
static bool writing = false, open_for_input = false;

int mus_audio_close(int line) 
{
  OSStatus err = noErr;
  UInt32 sizeof_running;
  UInt32 running;
  if (open_for_input)
    {
      in_buf = 0;
      err = AudioDeviceStop(device, (AudioDeviceIOProc)reader);
      if (err == noErr) 
	err = AudioDeviceRemoveIOProc(device, (AudioDeviceIOProc)reader);
    }
  else
    {
      if ((in_buf > 0) && (!writing))
	{
	  /* short enough sound that we never got started? */
	  err = AudioDeviceAddIOProc(device, (AudioDeviceIOProc)writer, NULL);
	  if (err == noErr)
	    err = AudioDeviceStart(device, (AudioDeviceIOProc)writer); /* writer will be called right away */
	  if (err == noErr)
	    writing = true;
	}
      if (writing)
	{
	  /* send out waiting buffers */
	  sizeof_running = sizeof(UInt32);
	  while (in_buf == out_buf)
	    {
	      err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyDeviceIsRunning, &sizeof_running, &running);
	    }
	  while (in_buf != out_buf)
	    {
	      err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyDeviceIsRunning, &sizeof_running, &running);
	    }
	  in_buf = 0;
	  err = AudioDeviceStop(device, (AudioDeviceIOProc)writer);
	  if (err == noErr) 
	    err = AudioDeviceRemoveIOProc(device, (AudioDeviceIOProc)writer);
	  writing = false;
	}
    }
  device = kAudioDeviceUnknown;
  if (err == noErr)
    return(MUS_NO_ERROR);
  return(MUS_ERROR);
}

typedef enum {CONVERT_NOT, CONVERT_COPY, CONVERT_SKIP, CONVERT_COPY_AND_SKIP, CONVERT_SKIP_N, CONVERT_COPY_AND_SKIP_N} audio_convert_t;
static audio_convert_t conversion_choice = CONVERT_NOT;
static float conversion_multiplier = 1.0;
static int dac_out_chans, dac_out_srate;
static int incoming_out_chans = 1, incoming_out_srate = 44100;
static int fill_point = 0;
static unsigned int bufsize = 0, current_bufsize = 0;

/* I'm getting bogus buffer sizes from the audio conversion stuff from Apple,
 *   and I think AudioConvert doesn't handle cases like 4->6 chans correctly
 *   so, I'll just do the conversions myself -- there is little need here
 *   for non-integer srate conversion anyway, and the rest is trivial.
 */

int mus_audio_open_output(int dev, int srate, int chans, int format, int size) 
{
  OSStatus err = noErr;
  UInt32 sizeof_device, sizeof_format, sizeof_bufsize;
  AudioStreamBasicDescription device_desc;
  sizeof_device = sizeof(AudioDeviceID);
  sizeof_bufsize = sizeof(unsigned int);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &sizeof_device, (void *)(&device));
  bufsize = 4096;
  if (err == noErr) 
    err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyBufferSize, &sizeof_bufsize, &bufsize);
  if (err != noErr) 
    {
      fprintf(stderr,"open audio output err: %d %s\n", (int)err, osx_error(err));
      return(MUS_ERROR);
    }
  /* now check for srate/chan mismatches and so on */
  sizeof_format = sizeof(AudioStreamBasicDescription);
  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyStreamFormat, &sizeof_format, &device_desc);
  if (err != noErr)
    {
      fprintf(stderr,"open audio output (get device format) err: %d %s\n", (int)err, osx_error(err));
      return(MUS_ERROR);
    }
  /* current DAC state: device_desc.mChannelsPerFrame, (int)(device_desc.mSampleRate) */
  /* apparently get stream format can return noErr but chans == 0?? */
  if ((device_desc.mChannelsPerFrame != chans) || 
      ((int)(device_desc.mSampleRate) != srate))
    {
      /* try to match DAC settings to current sound */
      device_desc.mChannelsPerFrame = chans;
      device_desc.mSampleRate = srate;
      device_desc.mBytesPerPacket = chans * 4; /* assume 1 frame/packet and float32 data */
      device_desc.mBytesPerFrame = chans * 4;
      sizeof_format = sizeof(AudioStreamBasicDescription);
      err = AudioDeviceSetProperty(device, 0, 0, false, kAudioDevicePropertyStreamFormat, sizeof_format, &device_desc);

      /* this error is bogus in some cases -- other audio systems just ignore it,
       *   but in my case (a standard MacIntel with no special audio hardware), if I leave
       *   this block out, the sound is played back at the wrong rate, and the volume
       *   of outa is set to 0.0?? 
       */

      if (err != noErr)
	{
	  /* it must have failed for some reason -- look for closest match available */
	  /* if srate = 22050 try 44100, if chans = 1 try 2 */
	  /* the "get closest match" business appears to be completely bogus... */
	  device_desc.mChannelsPerFrame = (chans == 1) ? 2 : chans;
	  device_desc.mSampleRate = (srate == 22050) ? 44100 : srate;
	  device_desc.mBytesPerPacket = device_desc.mChannelsPerFrame * 4; /* assume 1 frame/packet and float32 data */
	  device_desc.mBytesPerFrame = device_desc.mChannelsPerFrame * 4;
	  sizeof_format = sizeof(AudioStreamBasicDescription);
	  err = AudioDeviceSetProperty(device, 0, 0, false, kAudioDevicePropertyStreamFormat, sizeof_format, &device_desc);
	  if (err != noErr)
	    {
	      sizeof_format = sizeof(AudioStreamBasicDescription);
	      err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyStreamFormatMatch, &sizeof_format, &device_desc);
	      if (err == noErr)
		{
		  /* match suggests: device_desc.mChannelsPerFrame, (int)(device_desc.mSampleRate) */
		  /* try to set DAC to reflect that match */
		  /* a bug here in emagic 2|6 -- we can get 6 channel match, but then can't set it?? */
		  sizeof_format = sizeof(AudioStreamBasicDescription);
		  err = AudioDeviceSetProperty(device, 0, 0, false, kAudioDevicePropertyStreamFormat, sizeof_format, &device_desc);
		  if (err != noErr) 
		    {
		      /* no luck -- get current DAC settings at least */
		      sizeof_format = sizeof(AudioStreamBasicDescription);
		      AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyStreamFormat, &sizeof_format, &device_desc);
		    }
		}
	    }
	  else 
	    {
	      /* nothing matches? -- get current DAC settings */
	      sizeof_format = sizeof(AudioStreamBasicDescription);
	      AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyStreamFormat, &sizeof_format, &device_desc);
	    }
	}
    }
  /* now DAC claims it is ready for device_desc.mChannelsPerFrame, (int)(device_desc.mSampleRate) */
  dac_out_chans = device_desc.mChannelsPerFrame; /* use better variable names */
  dac_out_srate = (int)(device_desc.mSampleRate);
  open_for_input = false;
  if ((bufs == NULL) || (bufsize > current_bufsize))
    {
      int i;
      if (bufs)
	{
	  for (i = 0; i < MAX_BUFS; i++) FREE(bufs[i]);
	  FREE(bufs);
	}
      bufs = (char **)CALLOC(MAX_BUFS, sizeof(char *));
      for (i = 0; i < MAX_BUFS; i++)
	bufs[i] = (char *)CALLOC(bufsize, sizeof(char));
      current_bufsize = bufsize;
    }
  in_buf = 0;
  out_buf = 0;
  fill_point = 0;
  incoming_out_srate = srate;
  incoming_out_chans = chans;
  if (incoming_out_chans == dac_out_chans)
    {
      if (incoming_out_srate == dac_out_srate)
	{
	  conversion_choice = CONVERT_NOT;
	  conversion_multiplier = 1.0;
	}
      else 
	{
	  /* here we don't get very fancy -- assume dac/2=in */
	  conversion_choice = CONVERT_COPY;
	  conversion_multiplier = 2.0;
	}
    }
  else
    {
      if (incoming_out_srate == dac_out_srate)
	{
	  if ((dac_out_chans == 2) && (incoming_out_chans == 1)) /* the usual case */
	    {
	      conversion_choice = CONVERT_SKIP;
	      conversion_multiplier = 2.0;
	    }
	  else
	    {
	      conversion_choice = CONVERT_SKIP_N;
	      conversion_multiplier = ((float)dac_out_chans / (float)incoming_out_chans);
	    }
	}
      else 
	{
	  if ((dac_out_chans == 2) && (incoming_out_chans == 1)) /* the usual case */
	    {
	      conversion_choice = CONVERT_COPY_AND_SKIP;
	      conversion_multiplier = 4.0;
	    }
	  else
	    {
	      conversion_choice = CONVERT_COPY_AND_SKIP_N;
	      conversion_multiplier = ((float)dac_out_chans / (float)incoming_out_chans) * 2;
	    }
	}
    }
  return(MUS_NO_ERROR);
}

static void convert_incoming(char *to_buf, int fill_point, int lim, char *buf)
{
  int i, j, k, jc, kc, ic;
  switch (conversion_choice)
    {
    case CONVERT_NOT:
      /* no conversion needed */
      for (i = 0; i < lim; i++)
	to_buf[i + fill_point] = buf[i];
      break;
    case CONVERT_COPY:
      /* copy sample to mimic lower srate */
      for (i = 0, j = fill_point; i < lim; i += 8, j += 16)
	for (k = 0; k < 8; k++)
	  {
	    to_buf[j + k] = buf[i + k];
	    to_buf[j + k + 8] = buf[i + k];
	  }
      break;
    case CONVERT_SKIP:
      /* skip sample for empty chan */
      for (i = 0, j = fill_point; i < lim; i += 4, j += 8)
	for (k = 0; k < 4; k++)
	  {
	    to_buf[j + k] = buf[i + k];
	    to_buf[j + k + 4] = 0;
	  }
      break;
    case CONVERT_SKIP_N:
      /* copy incoming_out_chans then skip up to dac_out_chans */
      jc = dac_out_chans * 4;
      ic = incoming_out_chans * 4;
      for (i = 0, j = fill_point; i < lim; i += ic, j += jc)
	{
	  for (k = 0; k < ic; k++) to_buf[j + k] = buf[i + k];
	  for (k = ic; k < jc; k++) to_buf[j + k] = 0;
	}
      break;
    case CONVERT_COPY_AND_SKIP:
      for (i = 0, j = fill_point; i < lim; i += 4, j += 16)
	for (k = 0; k < 4; k++)
	  {
	    to_buf[j + k] = buf[i + k];
	    to_buf[j + k + 4] = 0;
	    to_buf[j + k + 8] = buf[i + k];
	    to_buf[j + k + 12] = 0;
	  }
      break;
    case CONVERT_COPY_AND_SKIP_N:
      /* copy for each active chan, skip rest */
      jc = dac_out_chans * 8;
      ic = incoming_out_chans * 4;
      kc = dac_out_chans * 4;
      for (i = 0, j = fill_point; i < lim; i += ic, j += jc)
	{
	  for (k = 0; k < ic; k++) 
	    {
	      to_buf[j + k] = buf[i + k];
	      to_buf[j + k + kc] = buf[i + k];	      
	    }
	  for (k = ic; k < kc; k++) 
	    {
	      to_buf[j + k] = 0;
	      to_buf[j + k + kc] = 0;
	    }
	}
      break;
    }
}

int mus_audio_write(int line, char *buf, int bytes) 
{
  OSStatus err = noErr;
  int lim, bp, out_bytes;
  UInt32 sizeof_running;
  UInt32 running;
  char *to_buf;
  to_buf = bufs[in_buf];
  out_bytes = (int)(bytes * conversion_multiplier);
  if ((fill_point + out_bytes) > bufsize)
    out_bytes = bufsize - fill_point;
  lim = (int)(out_bytes / conversion_multiplier);
  if (!writing)
    {
      convert_incoming(to_buf, fill_point, lim, buf);
      fill_point += out_bytes;
      if (fill_point >= bufsize)
	{
	  in_buf++;
	  fill_point = 0;
	  if (in_buf == MAX_BUFS)
	    {
	      in_buf = 0;
	      err = AudioDeviceAddIOProc(device, (AudioDeviceIOProc)writer, NULL);
	      if (err == noErr)
		err = AudioDeviceStart(device, (AudioDeviceIOProc)writer); /* writer will be called right away */
	      if (err == noErr)
		{
		  writing = true;
		  return(MUS_NO_ERROR);
		}
	      else return(MUS_ERROR);
	    }
	}
      return(MUS_NO_ERROR);
    }
  if ((fill_point == 0) && (in_buf == out_buf))
    {
      bp = out_buf;
      sizeof_running = sizeof(UInt32);
      while (bp == out_buf)
	{
	  /* i.e. just kill time without hanging */
	  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyDeviceIsRunning, &sizeof_running, &running);
	  /* usleep(10); */
	}
    }
  to_buf = bufs[in_buf];
  if (fill_point == 0) memset((void *)to_buf, 0, bufsize);
  convert_incoming(to_buf, fill_point, lim, buf);
  fill_point += out_bytes;
  if (fill_point >= bufsize)
    {
      in_buf++;
      fill_point = 0;
      if (in_buf >= MAX_BUFS) in_buf = 0;
    }
  return(MUS_NO_ERROR);
}

int mus_audio_open_input(int dev, int srate, int chans, int format, int size) 
{
  OSStatus err = noErr;
  UInt32 sizeof_device;
  UInt32 sizeof_bufsize;
  sizeof_device = sizeof(AudioDeviceID);
  sizeof_bufsize = sizeof(unsigned int);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &sizeof_device, (void *)(&device));
  bufsize = 4096;
  if (err == noErr) 
    err = AudioDeviceGetProperty(device, 0, true, kAudioDevicePropertyBufferSize, &sizeof_bufsize, &bufsize);
  if (err != noErr) 
    {
      fprintf(stderr,"open audio input err: %d %s\n", (int)err, osx_error(err));
      return(MUS_ERROR);
    }
  open_for_input = true;
  /* assume for now that recorder (higher level) will enforce match */
  if ((bufs == NULL) || (bufsize > current_bufsize))
    {
      int i;
      if (bufs)
	{
	  for (i = 0; i < MAX_BUFS; i++) FREE(bufs[i]);
	  FREE(bufs);
	}
      bufs = (char **)CALLOC(MAX_BUFS, sizeof(char *));
      for (i = 0; i < MAX_BUFS; i++)
	bufs[i] = (char *)CALLOC(bufsize, sizeof(char));
      current_bufsize = bufsize;
    }
  in_buf = 0;
  out_buf = 0;
  fill_point = 0;
  incoming_out_srate = srate;
  incoming_out_chans = chans;
  err = AudioDeviceAddIOProc(device, (AudioDeviceIOProc)reader, NULL);
  if (err == noErr)
    err = AudioDeviceStart(device, (AudioDeviceIOProc)reader);
  if (err != noErr) 
    {
      fprintf(stderr,"add open audio input err: %d %s\n", (int)err, osx_error(err));
      return(MUS_ERROR);
    }
  return(MUS_NO_ERROR);
}

int mus_audio_read(int line, char *buf, int bytes) 
{
  OSStatus err = noErr;
  int bp;
  UInt32 sizeof_running;
  UInt32 running;
  char *to_buf;
  if (in_buf == out_buf)
    {
      bp = out_buf;
      sizeof_running = sizeof(UInt32);
      while (bp == out_buf)
	{
	  err = AudioDeviceGetProperty(device, 0, true, kAudioDevicePropertyDeviceIsRunning, &sizeof_running, &running);
	  if (err != noErr) 
	    fprintf(stderr,"wait err: %s ", osx_error(err));
	}
    }
  to_buf = bufs[in_buf];
  if (bytes <= bufsize)
    memmove((void *)buf, (void *)to_buf, bytes);
  else memmove((void *)buf, (void *)to_buf, bufsize);
  in_buf++;
  if (in_buf >= MAX_BUFS) in_buf = 0;
  return(MUS_ERROR);
}

static int max_chans(AudioDeviceID device, int input)
{
  int maxc = 0, formats, k, config_chans;
  UInt32 size;
  OSStatus err;
  AudioStreamBasicDescription desc;
  AudioStreamBasicDescription *descs;
  size = sizeof(AudioStreamBasicDescription);
  err = AudioDeviceGetProperty(device, 0, input, kAudioDevicePropertyStreamFormat, &size, &desc);
  if (err == noErr) 
    {
      maxc = (int)(desc.mChannelsPerFrame);
      size = 0;
      err = AudioDeviceGetPropertyInfo(device, 0, input, kAudioDevicePropertyStreamFormats, &size, NULL);
      formats = size / sizeof(AudioStreamBasicDescription);
      if (formats > 1)
	{
	  descs = (AudioStreamBasicDescription *)CALLOC(formats, sizeof(AudioStreamBasicDescription));
	  size = formats * sizeof(AudioStreamBasicDescription);
	  err = AudioDeviceGetProperty(device, 0, input, kAudioDevicePropertyStreamFormats, &size, descs);
	  if (err == noErr) 
	    for (k = 0; k < formats; k++)
	      if ((int)(descs[k].mChannelsPerFrame) > maxc) maxc = (int)(descs[k].mChannelsPerFrame);
	  FREE(descs);
	}
    }
  else fprintf(stderr, "read chans hit: %s\n", osx_error(err));
  config_chans = max_chans_via_stream_configuration(device, input);
  if (config_chans > maxc) return(config_chans);
  return(maxc);
}

int mus_audio_mixer_read(int dev1, int field, int chan, float *val)
{
  AudioDeviceID dev = kAudioDeviceUnknown;
  OSStatus err = noErr;
  UInt32 size;
  Float32 amp;
  int i, curdev;
  bool in_case = false;
  switch (field) 
    {
    case MUS_AUDIO_AMP:   
      size = sizeof(AudioDeviceID);
      err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &dev);
      size = sizeof(Float32);
      err = AudioDeviceGetProperty(dev, chan + 1, false, kAudioDevicePropertyVolumeScalar, &size, &amp);
      if (err == noErr)
	val[0] = (Float)amp;
      else val[0] = 0.0;
      break;
    case MUS_AUDIO_CHANNEL: 
      curdev = MUS_AUDIO_DEVICE(dev1);
      size = sizeof(AudioDeviceID);
      in_case = ((curdev == MUS_AUDIO_MICROPHONE) || (curdev == MUS_AUDIO_LINE_IN));
      if (in_case)
	err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &size, &dev);
      else err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &dev);
      if (err != noErr) fprintf(stderr, "get default: %s\n", osx_error(err));
      val[0] = max_chans(dev, in_case);
      break;
    case MUS_AUDIO_SRATE: 
      val[0] = 44100;
      break;
    case MUS_AUDIO_FORMAT:
      /* never actually used except perhaps play.scm */
      val[0] = 1.0;
#if MUS_LITTLE_ENDIAN
      val[1] = MUS_LFLOAT;
#else
      val[1] = MUS_BFLOAT;
#endif
      break;
    case MUS_AUDIO_PORT:
      i = 0;
      if (1 < chan) val[1] = MUS_AUDIO_MICROPHONE;
      if (2 < chan) val[2] = MUS_AUDIO_DAC_OUT;
      val[0] = 2;
      break;
    case MUS_AUDIO_SAMPLES_PER_CHANNEL: 
      /* bufsize / 16: mulaw 22050 mono -> float 44100 stereo => 16:1 expansion */
      {
	int bufsize = 4096;
	UInt32 sizeof_bufsize;
	sizeof_bufsize = sizeof(unsigned int);
	curdev = MUS_AUDIO_DEVICE(dev1);
	size = sizeof(AudioDeviceID);
	in_case = ((curdev == MUS_AUDIO_MICROPHONE) || (curdev == MUS_AUDIO_LINE_IN));
	if (in_case)
	  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &size, &dev);
	else err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &dev);
	if (err != noErr) 
	  fprintf(stderr, "get samps/chan: %s\n", osx_error(err));
	else 
	  {
	    err = AudioDeviceGetProperty(dev, 0, true, kAudioDevicePropertyBufferSize, &sizeof_bufsize, &bufsize);
	    if (err == noErr) val[0] = (float)(bufsize / 16);
	  }
      }
      break;
    default: 
      return(MUS_ERROR);
      break;
    }
  return(MUS_NO_ERROR);
}

int mus_audio_mixer_write(int dev1, int field, int chan, float *val) 
{
  AudioDeviceID dev = kAudioDeviceUnknown;
  OSStatus err = noErr;
  Boolean writable;
  UInt32 size;
  Float32 amp;
  switch (field) 
    {
    case MUS_AUDIO_AMP:   
      size = sizeof(AudioDeviceID);
      err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, (void *)(&dev));
      err = AudioDeviceGetPropertyInfo(dev, chan + 1, false, kAudioDevicePropertyVolumeScalar, NULL, &writable); /* "false" -> output */
      amp = (Float32)(val[0]);
      if ((err == kAudioHardwareNoError) && (writable))
	err = AudioDeviceSetProperty(dev, NULL, chan + 1, false, kAudioDevicePropertyVolumeScalar, sizeof(Float32), &amp);
      break;
    default: 
      return(MUS_ERROR);
      break;
    }
  return(MUS_NO_ERROR);
}

int mus_audio_initialize(void) {return(MUS_NO_ERROR);}
int mus_audio_systems(void) {return(1);}
char *mus_audio_system_name(int system) {return("Mac OSX");}

char *mus_audio_moniker(void) {return("Mac OSX audio");}
#endif



/* -------------------------------- ESD -------------------------------- */

/* ESD audio IO for Linux                   *
 * Nick Bailey <nick@bailey-family.org.uk>  *
 * also n.bailey@elec.gla.ac.uk             */

/* ESD is pretty well undocumented, and I've not looked at snd before, *
 * but here goes...                                                    *
 *                                                                     *
 * History:                                                            *
 * 14th Nov 2000: copied SUN drivers here and started to hack.  NJB.   *
 *                                                                     */

#ifdef MUS_ESD
#define AUDIO_OK

#include <esd.h>

static int esd_play_sock = -1;
static int esd_rec_sock  = -1;
static char esd_name[] = "Enlightened Sound Daemon";
static int swap_end, resign; /* How to handle samples on write */

int mus_audio_initialize(void) {return(MUS_NO_ERROR);}
int mus_audio_systems(void) {return(1);}
char *mus_audio_system_name(int system) {return esd_name;}
static char our_name[LABEL_BUFFER_SIZE];
char *mus_audio_moniker(void) 
{
#ifdef MUS_ESD_VERSION
  #ifdef MUS_AUDIOFILE_VERSION
    mus_snprintf(our_name, LABEL_BUFFER_SIZE, "%s: %s (Audiofile %s)", esd_name, MUS_ESD_VERSION, MUS_AUDIOFILE_VERSION);
  #else
    mus_snprintf(our_name, LABEL_BUFFER_SIZE, "%s: %s", esd_name, MUS_ESD_VERSION);
  #endif
  return(our_name);
#else
  return(esd_name);
#endif
}

int mus_audio_api(void) {return(0);}

#define RETURN_ERROR_EXIT(Error_Type, Audio_Line, Ur_Error_Message) \
  do { char *Error_Message; Error_Message = Ur_Error_Message; \
    if (esd_play_sock != -1) close(esd_play_sock); \
    if (esd_rec_sock != -1) close(esd_rec_sock); \
    if (Error_Message) \
      {MUS_STANDARD_ERROR(Error_Type, Error_Message); FREE(Error_Message);} \
    else MUS_STANDARD_ERROR(Error_Type, mus_error_type_to_string(Error_Type)); \
    return(MUS_ERROR); \
  } while (false)

/* No we're laughing.  snd think's its talking to a real piece of hardware
   so it'll only try to open it once.  We can just use the socket numbers */

/* REVOLTING HACK!  to_esd_format is called from mus_audio_open, and
   /as a side effect/, sets a flag to tell the write routine whether
   or not to change the endienness of the audio sample data (afaik,
   esd can't do this for us).  Same goes for signed-ness.
   If it gets called from elsewhere, it could be nasty. */

static int to_esd_format(int snd_format)
{
  /* Try this on the Macs: it may be esd expects Bigendian on those */
  switch (snd_format) { /* Only some are supported */
  case MUS_UBYTE:   swap_end = 0; resign = 0; return ESD_BITS8;
  case MUS_LSHORT:  swap_end = 0; resign = 0; return ESD_BITS16;
  case MUS_BSHORT:  swap_end = 1; resign = 0; return ESD_BITS16;
  case MUS_ULSHORT: swap_end = 0; resign = 1; return ESD_BITS16;
  case MUS_UBSHORT: swap_end = 1; resign = 1; return ESD_BITS16;
  }
  return MUS_ERROR;
}

int mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size)
{
  int esd_prop = ESD_STREAM;
  int esd_format;

  if ((esd_format = to_esd_format(format)) == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_out,
		      mus_format("Can't handle format %d (%s) through esd",
				 format, mus_data_format_name(format)));
  else
    esd_prop |= esd_format;

  if (chans < 1 || chans > 2)
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_out,
		      mus_format("Can't handle format %d channels through esd",
				 format));
  else 
    esd_prop |= chans == 1 ? ESD_MONO : ESD_STEREO;

  esd_play_sock = esd_play_stream(esd_prop, srate,
				  NULL, "snd playback stream");

  if (esd_play_sock ==  -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, audio_out,
		      mus_format("Sonorus device %d (%s) not available",
				 ur_dev, mus_audio_device_name(ur_dev)));
  else
    return esd_play_sock;
}

int mus_audio_write(int line, char *buf, int bytes)
{
  int written;
  char *to = buf;

  /* Esd can't do endianness or signed/unsigned conversion,
     so it's our problem.  We won't screw up the callers data */

  if (swap_end) {
    char *from = buf;
    char *p;
    int samps = bytes/2;
    p = to = (char *)alloca(bytes);
    while (samps--) {
      *p++ = *(from+1);
      *p++ = *(from);
      from += 2;
    }
  }

  /* Need to do something about sign correction here */

  do {
    written = write(line, to, bytes);
    if (written > 0) {
      bytes -= written;
      to += written;
    }
    else
      RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			mus_format("write error: %s", strerror(errno)));
  } while (bytes > 0);
  return MUS_NO_ERROR;
}

int mus_audio_close(int line)
{
  esd_close(line);
  if (esd_play_sock == line) esd_play_sock = -1;
  else if (esd_rec_sock == line) esd_rec_sock = -1;
  return MUS_NO_ERROR;
}

int mus_audio_read(int line, char *buf, int bytes)
{
  int bytes_read;

  do {
    bytes_read = read(line, buf, bytes);
    if (bytes_read > 0) { /* 0 -> EOF; we'll regard that as an error */
      bytes -= bytes_read;
      buf += bytes_read;
    } else
      RETURN_ERROR_EXIT(MUS_AUDIO_WRITE_ERROR, -1,
			mus_format("read error: %s", strerror(errno)));
  } while (bytes > 0);
  return MUS_NO_ERROR;
}

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int size)
{
  int esd_prop = ESD_STREAM;
  int esd_format;

  if ((esd_format = to_esd_format(format)) == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, audio_out,
		      mus_format("Can't handle format %d (%s) through esd",
				 format, mus_data_format_name(format)));
  else
    esd_prop |= esd_format;

  if (chans < 1 || chans > 2)
    RETURN_ERROR_EXIT(MUS_AUDIO_CHANNELS_NOT_AVAILABLE, audio_out,
		      mus_format("Can't handle format %d channels through esd",
				 chans));
  else 
    esd_prop |= chans == 1 ? ESD_MONO : ESD_STEREO;

  esd_rec_sock = esd_play_stream(esd_prop, srate,
				  NULL, "snd record stream");

  if (esd_rec_sock ==  -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_DEVICE_NOT_AVAILABLE, audio_out,
		      mus_format("Device %d (%s) not available",
				 ur_dev, mus_audio_device_name(ur_dev)));
  else
    return esd_rec_sock;
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val) 
{
  /* Not really sure what to do here.  Mixer is at the other end of the
     socket.  Needs work.  NJB */

  /* int card = MUS_AUDIO_SYSTEM(ur_dev); */
    int device = MUS_AUDIO_DEVICE(ur_dev);

    if (device == MUS_AUDIO_MIXER) {
	val[0] = 0.0;
	return MUS_NO_ERROR;
    }

    if (field == MUS_AUDIO_PORT) {
	val[0] = 1.0;
	return MUS_NO_ERROR;
    }

    switch (field) {
    case MUS_AUDIO_AMP: 
      /* amplitude value */
      val[0] = 1.0;
      break;
    case MUS_AUDIO_SAMPLES_PER_CHANNEL: 
      val[0] = 44100;
      break;
    case MUS_AUDIO_CHANNEL: 
      /* number of channels */
      val[0] = 2.0; 
      if (chan > 1) {
	val[1] = 1.0; 
	val[2] = 2.0; 
      }
      break;
    case MUS_AUDIO_SRATE: 
      /* supported sample rates */
      val[0] = 44100;
      if (chan > 1) {
	val[1] = 8000; 
	val[2] = 48000; 
      }
      break;
    case MUS_AUDIO_FORMAT:
      /* supported formats (ugly...) */
      val[0] = 3.0;
      val[1] = MUS_UBYTE;
      val[2] = MUS_LSHORT;
      val[3] = MUS_BSHORT;
      break;

    case MUS_AUDIO_DIRECTION: /* Needs sorting.  NJB */ 
      /* 0-->playback, 1-->capture */
      val[0] = 0;
      break;

    default: 
      RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1, NULL);
      /* return(mus_error(MUS_AUDIO_CANT_READ, NULL)); */ /* Bill 14-Nov-02 -- avoid possibly uncaught throw */
      break;
    }
    return(MUS_NO_ERROR);
}


int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val) 
{
  /* Ditto */
  val[0] = 0.0;
  return MUS_NO_ERROR;
}

/* pause can be implemented with play.pause and record.pause */


void describe_audio_state_1(void)
{
  pprint("Enlightened Sound Daemon via socket connexion to default host");
}

#endif


/* ------------------------------- JACK ----------------------------------------- */

/* Kjetil S. Matheussen. k.s.matheussen@notam02.no */
/* Based on code from ceres. */
 
#if HAVE_JACK
#define AUDIO_OK
#include <jack/jack.h>
#include <samplerate.h>
#include <sys/mman.h>
#include <signal.h>

#if MUS_LITTLE_ENDIAN
#  define MUS_COMP_SHORT MUS_LSHORT
#  define MUS_COMP_FLOAT MUS_LFLOAT
#else
#  define MUS_COMP_SHORT MUS_BSHORT
#  define MUS_COMP_FLOAT MUS_BFLOAT
#endif

#define SRC_QUALITY SRC_SINC_BEST_QUALITY
 
/*************/
/* Jack Part */
/*************/

#define SNDJACK_NUMINCHANNELS 4

#define SNDJACK_MAXSNDS 20

#define SNDJACK_BUFFERSIZE 32768

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

struct SndjackChannel{
  jack_port_t *port;
  sample_t *buffer;
};

static jack_client_t *sndjack_client = NULL;


/*************************/
/* Variables for reading */
/*************************/
static int sndjack_num_read_channels_allocated=0;
static int sndjack_num_read_channels_inuse=0;
static struct SndjackChannel *sndjack_read_channels=NULL;
static pthread_cond_t sndjack_read_cond= PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sndjack_read_mutex= PTHREAD_MUTEX_INITIALIZER;
static int sj_r_buffersize=0;
static int sj_r_writeplace=0;
static int sj_r_readplace=0;
static int sj_r_unread=0;
static int sj_r_xrun=0;
static int sj_r_totalxrun=0;

/*************************/
/* Variables for writing */
/*************************/
static pthread_cond_t sndjack_cond= PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sndjack_mutex=  PTHREAD_MUTEX_INITIALIZER;

enum{SJ_STOPPED,SJ_RUNNING,SJ_ABOUTTOSTOP};

// Variables for the ringbuffer:
static  int sj_writeplace=0;
static  int sj_readplace=0;
static  int sj_unread=0;
static  int sj_buffersize;
static int sj_jackbuffersize; // number of frames sent to sndjack_process.
static int sj_totalxrun=0;
static int sj_xrun=0;
static int sj_status=SJ_STOPPED;

static int sndjack_num_channels_allocated=0;
static int sndjack_num_channels_inuse=0;
static struct SndjackChannel *sndjack_channels=NULL;
static int sndjack_read_format;

static SRC_STATE **sndjack_srcstates;
static double sndjack_srcratio=1.0;

static int jack_mus_watchdog_counter=0;


#define SJ_MAX(a,b) (((a)>(b))?(a):(b))

static void sndjack_read_process(jack_nframes_t nframes){
  int i,ch;
  sample_t *out[sndjack_num_channels_allocated];

  if(sndjack_num_read_channels_inuse==0) return;

  for(ch=0;ch<sndjack_num_read_channels_allocated;ch++){
    out[ch]=(sample_t*)jack_port_get_buffer(sndjack_read_channels[ch].port,nframes);
  }

  for(i=0;i<nframes;i++){
    if(sj_r_unread==sj_buffersize){
      sj_r_xrun+=nframes-i;
      goto exit;
    }
    for(ch=0;ch<sndjack_num_read_channels_inuse;ch++)
      sndjack_read_channels[ch].buffer[sj_r_writeplace]=out[ch][i];
    sj_r_unread++;
    sj_r_writeplace++;
    if(sj_r_writeplace==sj_r_buffersize)
      sj_r_writeplace=0;
  }
 exit:
  pthread_cond_broadcast(&sndjack_read_cond);
}


static void sndjack_write_process(jack_nframes_t nframes){
  int ch,i;
  sample_t *out[sndjack_num_channels_allocated];

  for(ch=0;ch<sndjack_num_channels_allocated;ch++){
    out[ch]=(sample_t*)jack_port_get_buffer(sndjack_channels[ch].port,nframes);
  }

  if(sj_status==SJ_STOPPED){
    for(ch=0;ch<sndjack_num_channels_allocated;ch++){
      memset(out[ch],0,nframes*sizeof(sample_t));
    }
  }else{

    // First null out unused channels, if any.
    if(sndjack_num_channels_inuse==1 && sndjack_num_channels_allocated>=2){
      for(ch=2;ch<sndjack_num_channels_allocated;ch++){
	memset(out[ch],0,nframes*sizeof(sample_t));
      }
    }else{
      for(ch=sndjack_num_channels_inuse;ch<sndjack_num_channels_allocated;ch++){
	memset(out[ch],0,nframes*sizeof(sample_t));
      }
    }

    for(i=0;i<nframes;i++){
      if(sj_unread==0){	
	if(sj_status==SJ_RUNNING)
	  sj_xrun+=nframes-i;
	for(;i<nframes;i++){
	  for(ch=0;ch<sndjack_num_channels_inuse;ch++){
	    out[ch][i]=0.0f;
	  }
	}
	break;
      }

      if(sndjack_num_channels_inuse==1 && sndjack_num_channels_allocated>=2){
	for(ch=0;ch<2;ch++){
	  out[ch][i]=sndjack_channels[0].buffer[sj_readplace];
	}
      }else{
	for(ch=0;ch<sndjack_num_channels_inuse;ch++){
	  out[ch][i]=sndjack_channels[ch].buffer[sj_readplace];
	}
      }
      sj_unread--;
      sj_readplace++;
      if(sj_readplace==sj_buffersize)
	sj_readplace=0;
    }
    
    pthread_cond_broadcast(&sndjack_cond);

    if(sj_status==SJ_ABOUTTOSTOP && sj_unread==0)
      sj_status=SJ_STOPPED;
  }

}
 


static int sndjack_process(jack_nframes_t nframes, void *arg){
  sndjack_read_process(nframes);
  sndjack_write_process(nframes);
  return 0;
}


static int sndjack_read(void *buf,int bytes,int chs){
  int i,ch;
  int nframes=bytes /
    sndjack_read_format==MUS_COMP_FLOAT ? sizeof(float) :
    sndjack_read_format==MUS_COMP_SHORT ? sizeof(short) :
    1;
  float *buf_f=buf;
  short *buf_s=buf;
  char *buf_c=buf;

  for(i=0;i<nframes;i++){
    while(sj_r_unread==0){
      pthread_cond_wait(&sndjack_read_cond,&sndjack_read_mutex);
      jack_mus_watchdog_counter++;
    }

    if(sj_r_xrun>0){
      sj_r_totalxrun+=sj_r_xrun;
      sj_r_xrun=0;
      return -1;
    }
    for(ch=0;ch<chs;ch++){
      switch(sndjack_read_format){
      case MUS_BYTE:
	buf_c[i*chs+ch]=sndjack_read_channels[ch].buffer[sj_r_readplace] * 127.9f;
	break;
      case MUS_COMP_SHORT:
	buf_s[i*chs+ch]=sndjack_read_channels[ch].buffer[sj_r_readplace] * 32767.9f;
	break;
      case MUS_COMP_FLOAT:
	buf_f[i*chs+ch]=sndjack_read_channels[ch].buffer[sj_r_readplace];
	break;
      }}
    sj_r_unread--;
    sj_r_readplace++;
    if(sj_r_readplace==sj_r_buffersize)
      sj_r_readplace=0;
  }
  return 0;
}

static void sndjack_write(sample_t **buf,int nframes,int latencyframes,int chs){
  int ch;
  int i;

  if(sj_xrun>0){
    if(sj_status==SJ_RUNNING){
      printf("Warning. %d frames delayed.\n",sj_xrun);
      sj_totalxrun+=sj_xrun;
    }
    sj_xrun=0;
  }

  for(i=0;i<nframes;i++){
    while(
	  sj_status==SJ_RUNNING
	  && (sj_unread==sj_buffersize
	      || sj_unread >= SJ_MAX(sj_jackbuffersize*2, latencyframes))
	  )
      {
	jack_mus_watchdog_counter++;
	pthread_cond_wait(&sndjack_cond,&sndjack_mutex);
      }

    for(ch=0;ch<chs;ch++)
      sndjack_channels[ch].buffer[sj_writeplace]=buf[ch][i];

    sj_unread++;
    sj_writeplace++;
    if(sj_writeplace==sj_buffersize)
      sj_writeplace=0;
  }

  if(sj_status==SJ_STOPPED)
    if(sj_unread>=sj_jackbuffersize)
      sj_status=SJ_RUNNING;
}
 
static int sndjack_buffersizecallback(jack_nframes_t nframes, void *arg){
  sj_jackbuffersize=nframes;
  return 0;
}

static int sndjack_getnumoutchannels(void){
  int lokke=0;
  const char **ports=jack_get_ports(sndjack_client,NULL,NULL,JackPortIsPhysical|JackPortIsInput);
  while(ports!=NULL && ports[lokke]!=NULL){
    lokke++;
  }
  if(lokke<2) return 2;
  return lokke;
}

static int sndjack_getnuminchannels(void){
  int lokke=0;
  const char **ports=jack_get_ports(sndjack_client,NULL,NULL,JackPortIsPhysical|JackPortIsOutput);
  while(ports!=NULL && ports[lokke]!=NULL){
    lokke++;
  }
  if(lokke<2) return 2;
  return lokke;
}


static int sndjack_init(void){
  int ch;
  int numch;
  int numch_read;
  int num=0;

  while(num<SNDJACK_MAXSNDS){
    char temp[500];
    sprintf(temp,"sndlib%d",num);
    if ((sndjack_client=jack_client_new(temp)) != 0) {
      break;
    }
    num++;
  }

  if(sndjack_client==NULL){
    /* printf("Unable to create new jack_client\n"); */
    return -1;
  }

  pthread_mutex_init(&sndjack_mutex,NULL);
  pthread_cond_init(&sndjack_cond,NULL);
  pthread_mutex_init(&sndjack_read_mutex,NULL);
  pthread_cond_init(&sndjack_read_cond,NULL);

  jack_set_process_callback(sndjack_client,sndjack_process,NULL);

  sndjack_num_channels_allocated = numch = sndjack_getnumoutchannels();
  numch_read=sndjack_getnuminchannels();
  sndjack_num_read_channels_allocated=SJ_MAX(SNDJACK_NUMINCHANNELS,numch_read);

  sndjack_channels=calloc(sizeof(struct SndjackChannel),numch);
  sndjack_read_channels=calloc(sizeof(struct SndjackChannel),sndjack_num_read_channels_allocated);

  for(ch=0;ch<numch;ch++){
    sndjack_channels[ch].buffer=calloc(sizeof(sample_t),SNDJACK_BUFFERSIZE);
  }
  for(ch=0;ch<sndjack_num_read_channels_allocated;ch++){
    sndjack_read_channels[ch].buffer=calloc(sizeof(sample_t),SNDJACK_BUFFERSIZE);
  }
  sj_buffersize=SNDJACK_BUFFERSIZE;

  for(ch=0;ch<numch;ch++){
    char temp[500];
    sprintf(temp,"out_%d",ch+1);
    if((sndjack_channels[ch].port=jack_port_register(
						     sndjack_client,
						     strdup(temp),
						     JACK_DEFAULT_AUDIO_TYPE,
						     JackPortIsOutput,
						     0
						     ))==NULL)
      {
	fprintf(stderr,"Error. Could not register jack port.\n");
	goto failed_register;
      }
  }

  for(ch=0;ch<sndjack_num_read_channels_allocated;ch++){
    char temp[500];
    sprintf(temp,"in_%d",ch+1);
    if((sndjack_read_channels[ch].port=jack_port_register(
							  sndjack_client,
							  strdup(temp),
							  JACK_DEFAULT_AUDIO_TYPE,
							  JackPortIsInput,
							  0
							  ))==NULL)
      {
	fprintf(stderr,"Error. Could not register jack port.\n");
	goto failed_register;
      }
  }




  sj_jackbuffersize=jack_get_buffer_size(sndjack_client);
  jack_set_buffer_size_callback(sndjack_client,sndjack_buffersizecallback,NULL);

  if (jack_activate (sndjack_client)) {
    fprintf (stderr, "Error. Cannot activate jack client.\n");
    goto failed_activate;
  }

  {
    const char **outportnames=jack_get_ports(sndjack_client,NULL,NULL,JackPortIsPhysical|JackPortIsInput);
    for(ch=0;outportnames && outportnames[ch]!=NULL && ch<numch;ch++){
      if (
	  jack_connect(
		       sndjack_client,
		       jack_port_name(sndjack_channels[ch].port),
		       outportnames[ch]
		       )
	  )
	{
	  printf ("Warning. Cannot connect jack output port %d: \"%s\".\n",ch,outportnames[ch]);
	}
    }
  }

  {    
    const char **inportnames=jack_get_ports(sndjack_client,NULL,NULL,JackPortIsPhysical|JackPortIsOutput);
    for(ch=0;inportnames && inportnames[ch]!=NULL && ch<numch;ch++){
    if (
	jack_connect(
		     sndjack_client,
		     inportnames[ch],
		     jack_port_name(sndjack_read_channels[ch].port)
		     )
	)
      {
	printf ("Warning. Cannot connect jack input port %d: \"%s\".\n",ch,inportnames[ch]);
      }
    }
  }
  return 0;
  
  // failed_connect:
 failed_activate:
  jack_deactivate(sndjack_client);
  
 failed_register:
  jack_client_close(sndjack_client);
  sndjack_client=NULL;

  return -1;
}
static void sndjack_cleanup(void){
  int ch;
  for(ch=0;ch<sndjack_num_channels_allocated;ch++){
    src_delete(sndjack_srcstates[ch]);
  }
  jack_deactivate(sndjack_client);
  jack_client_close(sndjack_client);

}



/***************/
/* Sndlib Part */
/***************/

static int sndjack_format;
static sample_t **sndjack_buffer;
static sample_t *sndjack_srcbuffer;

static int sndjack_dev;
static int sndjack_read_dev;

/* prototypes for the jack sndlib functions */
static int   jack_mus_audio_initialize(void);
static void  jack_mus_oss_set_buffers(int num, int size);
static int   jack_mus_audio_systems(void);
static char* jack_mus_audio_system_name(int system);
static char* jack_mus_audio_moniker(void);
static int   jack_mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size);
static int   jack_mus_audio_open_input(int ur_dev, int srate, int chans, int format, int requested_size);
static int   jack_mus_audio_write(int id, char *buf, int bytes);
static int   jack_mus_audio_read(int id, char *buf, int bytes);
static int   jack_mus_audio_close(int id);
static int   jack_mus_audio_mixer_read(int ur_dev, int field, int chan, float *val);
static int   jack_mus_audio_mixer_write(int ur_dev, int field, int chan, float *val);
static void  jack_describe_audio_state_1(void);


static int jack_mus_audio_initialize(void) {
  int ch;

  if(audio_initialized){
    return MUS_NO_ERROR;
  }

  if(sndjack_init()!=0)
    return MUS_ERROR;

  sndjack_buffer=calloc(sizeof(sample_t*),sndjack_num_channels_allocated);
  for(ch=0;ch<sndjack_num_channels_allocated;ch++)
    sndjack_buffer[ch]=calloc(sizeof(sample_t),SNDJACK_BUFFERSIZE);
  sndjack_srcbuffer=calloc(sizeof(sample_t),SNDJACK_BUFFERSIZE);

  sndjack_srcstates=calloc(sizeof(SRC_STATE*),sndjack_num_channels_allocated);
  for(ch=0;ch<sndjack_num_channels_allocated;ch++){
    sndjack_srcstates[ch]=src_new(SRC_QUALITY,1,NULL);
  }

  atexit(sndjack_cleanup);

  api = JACK_API;
  vect_mus_audio_initialize = jack_mus_audio_initialize;
  vect_mus_oss_set_buffers = jack_mus_oss_set_buffers;
  vect_mus_audio_systems = jack_mus_audio_systems;
  vect_mus_audio_system_name = jack_mus_audio_system_name;
  vect_mus_audio_moniker = jack_mus_audio_moniker;
  vect_mus_audio_open_output = jack_mus_audio_open_output;
  vect_mus_audio_open_input = jack_mus_audio_open_input;
  vect_mus_audio_write = jack_mus_audio_write;
  vect_mus_audio_read = jack_mus_audio_read;
  vect_mus_audio_close = jack_mus_audio_close;
  vect_mus_audio_mixer_read = jack_mus_audio_mixer_read;
  vect_mus_audio_mixer_write = jack_mus_audio_mixer_write;
  vect_describe_audio_state_1 = jack_describe_audio_state_1;

  audio_initialized = true;

#if 0  

  /* Locking all future memory shouldn't be that necessary, and might even freeze the machine in certain situations. */
  /* So remove MCL_FUTURE from the mlockall call. (No. We can't do that. It can screw up code using the realtime extension. -Kjetil.*/
  munlockall();
  //mlockall(MCL_CURRENT);
  
  // Instead we just do this: (which is not enough, but maybe better than nothing)
  {
    mlock(sndjack_channels,sizeof(struct SndjackChannel)*sndjack_num_channels_allocated);
    mlock(sndjack_read_channels,sizeof(struct SndjackChannel)*sndjack_num_read_channels_allocated);

    for(ch=0;ch<numch;ch++){
      mlock(sndjack_channels[ch].buffer,sizeof(sample_t)*SNDJACK_BUFFERSIZE);
    }
    for(ch=0;ch<sndjack_num_read_channels_allocated;ch++){
      mlock(sndjack_read_channels[ch].buffer,sizeof(sample_t)*SNDJACK_BUFFERSIZE);
    }
  }
#endif

  return MUS_NO_ERROR;
}

// ??
static void  jack_mus_oss_set_buffers(int num, int size){
}

static int jack_mus_isrunning=0;
static pid_t jack_mus_player_pid;  
static pthread_t jack_mus_watchdog_thread;

static void *jack_mus_audio_watchdog(void *arg){
  struct sched_param par;

  par.sched_priority = sched_get_priority_max(SCHED_RR);
  if(sched_setscheduler(0,SCHED_RR,&par)==-1){
    fprintf(stderr,"SNDLIB: Unable to set SCHED_RR realtime priority for the watchdog thread. No watchdog.\n");
    goto exit;
  }

  for(;;){
    int last=jack_mus_watchdog_counter;
    sleep(1);

    if(jack_mus_isrunning && jack_mus_watchdog_counter<last+10){
      struct sched_param par;
      fprintf(stderr,"SNDLIB: Setting player to non-realtime for 2 seconds.\n");

      par.sched_priority = 0;
      if(sched_setscheduler(jack_mus_player_pid,SCHED_OTHER,&par)==-1){
	fprintf(stderr,"SNDLIB: Unable to set non-realtime priority. Must kill player thread. Sorry!\n");
	while(1){
	  kill(jack_mus_player_pid,SIGKILL);
	  sleep(2);
	}
      }

      sleep(2);

      if(jack_mus_isrunning){
	par.sched_priority = sched_get_priority_min(SCHED_RR)+1;
	if(sched_setscheduler(jack_mus_player_pid,SCHED_RR,&par)==-1){
	  fprintf(stderr,"SNDLIB: Could not set back to realtime priority...\n");
	}else
	  fprintf(stderr,"SNDLIB: Play thread set back to realtime priority.\n");
      }

    }
  }
 exit:
  fprintf(stderr,"SNDLIB: Watchdog exiting\n");
  return NULL;
}



static void jack_mus_audio_set_realtime(void){
  struct sched_param par;
  static int watchdog_started=0;

  jack_mus_player_pid=getpid();

  if(watchdog_started==0){
    if(pthread_create(&jack_mus_watchdog_thread,NULL,jack_mus_audio_watchdog,NULL)!=0){
      fprintf(stderr,"Could not create watchdog. Not running realtime\n");
      return;
    }
    watchdog_started=1;
  }

  jack_mus_isrunning=1;

  par.sched_priority = sched_get_priority_min(SCHED_RR)+1;
  if(sched_setscheduler(0,SCHED_RR,&par)==-1){
    fprintf(stderr,"SNDLIB: Unable to set SCHED_RR realtime priority for the player thread.\n");
  }{
    //fprintf(stderr,"Set realtime priority\n");
  }
}

static void jack_mus_audio_set_non_realtime(void){
  struct sched_param par;
  par.sched_priority = 0;
  sched_setscheduler(0,SCHED_OTHER,&par);
  //fprintf(stderr,"Set non-realtime priority\n");
  jack_mus_isrunning=0;
}

int jack_mus_audio_open_output(int dev, int srate, int chans, int format, int size){
  if(sndjack_client==NULL){
    if(jack_mus_audio_initialize()==MUS_ERROR)
      return MUS_ERROR;
  }
  
  if(sndjack_num_channels_allocated<chans){
    printf("Error. Can not play back %d channels. (Only %d)\n",chans,sndjack_num_channels_allocated);
    return MUS_ERROR;
  }

  if(format!=MUS_BYTE && format!=MUS_COMP_SHORT && format!=MUS_COMP_FLOAT){
    printf("Error, unable to handle format %s.\n",mus_data_format_to_string(format));
    return MUS_ERROR;
  }

  while(sj_status!=SJ_STOPPED) usleep(5);

  sj_unread=0;
  sj_writeplace=0;
  sj_readplace=0;


  if(srate!=jack_get_sample_rate(sndjack_client)){
    int lokke;
    //printf("Warning, sample-rate differs between snd and jack. Sound will not be played correctly! %d/%d\n",srate,jack_get_sample_rate(sndjack_client));
    sndjack_srcratio=(double)jack_get_sample_rate(sndjack_client)/(double)srate;
    for(lokke=0;lokke<chans;lokke++){
      src_reset(sndjack_srcstates[lokke]);
    }
  }else{
    sndjack_srcratio=1.0;
  }

  sndjack_format=format;
  sndjack_num_channels_inuse=chans;
  sndjack_dev=dev;

  jack_mus_audio_set_realtime();

  return(MUS_NO_ERROR);
}
 
static int sndjack_from_byte(int ch,int chs,char *buf,float *out,int bytes){
  int i;
  int len=bytes/chs;
  if(len>SNDJACK_BUFFERSIZE) return -1;

  for(i=0;i<len;i++){
    out[i]=MUS_BYTE_TO_SAMPLE(buf[i*chs+ch]);
  }
  return len;
}

static int sndjack_from_short(int ch,int chs,short *buf,float *out,int bytes){
  int i;
  int len=bytes/(sizeof(short)*chs);
  if(len>SNDJACK_BUFFERSIZE) return -1;

  for(i=0;i<len;i++){
    out[i]=(float)buf[i*chs+ch]/32768.1f;
  }
  return len;
}

static int sndjack_from_float(int ch,int chs,float *buf,float *out,int bytes){
  int i;
  int len=bytes/(sizeof(float)*chs);
  if(len>SNDJACK_BUFFERSIZE) return -1;

  for(i=0;i<len;i++){
    out[i]=buf[i*chs+ch];
  }
  return len;
}


int jack_mus_audio_write(int line, char *buf, int bytes){
  int i;
  int ch;
  int outlen=0;

  for(ch=0;ch<sndjack_num_channels_inuse;ch++){
    int len = 0;
    float *buf2=sndjack_srcratio==1.0?sndjack_buffer[ch]:sndjack_srcbuffer;

    switch(sndjack_format){
    case MUS_BYTE:
      len=sndjack_from_byte(ch,sndjack_num_channels_inuse,buf,buf2,bytes);
      break;
    case MUS_COMP_SHORT:
      len=sndjack_from_short(ch,sndjack_num_channels_inuse,(short *)buf,buf2,bytes);
      break;
    case MUS_COMP_FLOAT:
      len=sndjack_from_float(ch,sndjack_num_channels_inuse,(float *)buf,buf2,bytes);
      break;
    }
    if(len<0){
      printf("Errur. Input buffer to large for mus_audio_write.\n");
      return MUS_ERROR;
    }

    if(sndjack_srcratio!=1.0){
      SRC_DATA src_data={
	buf2,sndjack_buffer[ch],
	len,SNDJACK_BUFFERSIZE,
	0,0,
	0,
	sndjack_srcratio
      };
      int res=src_process(sndjack_srcstates[ch],&src_data);
      if(res!=0){
	printf("Error while resampling. (%s)\n",src_strerror(res));
	return MUS_ERROR;
      }
      if(src_data.input_frames!=len){
	printf("Unsuccessfull resampling: Should have used %d bytes, used %d.",len,src_data.input_frames);
	return MUS_ERROR;
      }
      if(ch>0 && src_data.output_frames_gen!=outlen){
	printf("Error, src_process did not output the same number of frames as previous resampled channel (%d/%d).\n"
	       "Please report this problem to k.s.matheussen@notam02.no. Thanks!\n",src_data.output_frames_gen,outlen);
	return MUS_ERROR;
      }
      outlen=src_data.output_frames_gen;
    }else{
      outlen=len;
    }
  }


  sndjack_write(sndjack_buffer,outlen,outlen*2,sndjack_num_channels_inuse);

  return MUS_NO_ERROR;
}
 
int jack_mus_audio_close(int line) 
{
  jack_mus_audio_set_non_realtime();
  if(line==sndjack_dev){
    sj_status=SJ_ABOUTTOSTOP;
    sndjack_num_channels_inuse=0;
  }
  return MUS_NO_ERROR;
 }

int jack_mus_audio_mixer_read(int dev, int field, int chan, float *val) 
{
  //printf("dev: %d, field: %d, chan: %d\n",dev,field,chan);

  switch(field){
  case MUS_AUDIO_FORMAT:
    val[1]=MUS_COMP_FLOAT;
    val[0]=1;
    break;
  case MUS_AUDIO_PORT:
    val[0]=1;
    val[1]=MUS_AUDIO_DIGITAL_IN;
    break;
  case MUS_AUDIO_CHANNEL:
    val[0]=sndjack_num_read_channels_allocated;
    break;
  case MUS_AUDIO_AMP: 
    val[0] = 1.0f;
    break;
  default:
    printf("Got unknown request with field %d %d\n",field, MUS_AUDIO_AMP);
    return MUS_ERROR;
  }

  return MUS_NO_ERROR;
}

int jack_mus_audio_mixer_write(int dev, int field, int chan, float *val){
  return(MUS_NO_ERROR);
}

int jack_mus_audio_open_input(int dev, int srate, int chans, int format, int size){
  if(sndjack_client==NULL){
    if(jack_mus_audio_initialize()==MUS_ERROR)
      return MUS_ERROR;
  }
  
  if(sndjack_num_read_channels_allocated<chans){
    printf("Error. Can not record %d channels. (Only %d)\n",chans,sndjack_num_read_channels_allocated);
    return MUS_ERROR;
  }

  printf("dev: %d\n" ,dev);
  if(format!=MUS_BYTE && format!=MUS_COMP_SHORT && format!=MUS_COMP_FLOAT){
    printf("Error, unable to handle format %s.\n",mus_data_format_to_string(format));
    return MUS_ERROR;
  }

  if(srate!=jack_get_sample_rate(sndjack_client)){
    printf("Warning, jacks samplerate is %d (and not %d), and the recording will use this samplerate too.\n",jack_get_sample_rate(sndjack_client),srate);
  }

  sndjack_read_format=format;
  sndjack_num_read_channels_inuse=chans;
  sndjack_read_dev=dev;

  return(MUS_NO_ERROR);
}

int jack_mus_audio_read(int line, char *buf, int bytes){
  if(sndjack_read(buf,bytes,sndjack_num_read_channels_inuse)==-1)
    return(MUS_ERROR);
  return MUS_NO_ERROR;
}


static void jack_describe_audio_state_1(void) {
  char temp[500];

  pprint("jack audio:\n");
  sprintf(temp,"\tNumber of output channels: %d\n",sndjack_num_channels_allocated);pprint(temp);
  sprintf(temp,"\tNumber of input channels: %d\n",sndjack_num_read_channels_allocated);pprint(temp);
  sprintf(temp,"\tSamplerate: %d\n",jack_get_sample_rate(sndjack_client));pprint(temp);
  sprintf(temp,"\tJack buffersize: %d\n",sj_jackbuffersize);pprint(temp);
  sprintf(temp,"\tSndjack buffersize: %d\n",SNDJACK_BUFFERSIZE);pprint(temp);
  sprintf(temp,"\tMax number of instances: %d\n",SNDJACK_MAXSNDS);pprint(temp);
  sprintf(temp,"\tTotal number of frames delayed: %d\n",sj_totalxrun);pprint(temp);
  sprintf(temp,"\tCurrent cpu-load: %f\n",jack_cpu_load(sndjack_client));pprint(temp);
  sprintf(temp,"\tIs running realtime: %s\n",jack_is_realtime(sndjack_client)==1?"yes":"no");pprint(temp);
  sprintf(temp,"\tResample quality (only used when needed): %s (%s)\n",src_get_name(SRC_QUALITY),src_get_description(SRC_QUALITY));pprint(temp);
  sprintf(temp,"\tIs able to handle the following audio formats: %s %s %s\n",mus_data_format_to_string(MUS_BYTE),mus_data_format_to_string(MUS_COMP_SHORT),mus_data_format_to_string(MUS_COMP_FLOAT));pprint(temp);
  sprintf(temp,"\tPrefered audio format: %s\n",mus_data_format_to_string(MUS_COMP_FLOAT));pprint(temp);
}


int jack_mus_audio_systems(void) {
  return(2);
}

char *jack_mus_audio_system_name(int system) {return("linux jack");}
char *jack_mus_audio_moniker(void) {return("jack");}
#endif
 


/* ------------------------------- HPUX ----------------------------------------- */

/* if this is basically the same as the Sun case with different macro names,
 * then it could perhaps be updated to match the new Sun version above --
 * Sun version changed 28-Jan-99
 */

#if defined(MUS_HPUX) && (!(defined(AUDIO_OK)))
#define AUDIO_OK
#include <sys/audio.h>

#define RETURN_ERROR_EXIT(Error_Type, Audio_Line, Ur_Error_Message) \
  do { char *Error_Message; Error_Message = Ur_Error_Message; \
    if (Audio_Line != -1) close(Audio_Line); \
    if (Error_Message) \
      {MUS_STANDARD_ERROR(Error_Type, Error_Message); FREE(Error_Message);} \
    else MUS_STANDARD_ERROR(Error_Type, mus_error_type_to_string(Error_Type)); \
    return(MUS_ERROR); \
  } while (false)

char *mus_audio_moniker(void) {return("HPUX audio");}

int mus_audio_open_output(int ur_dev, int srate, int chans, int format, int size)
{
  int fd, i, dev;
  struct audio_describe desc;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  fd = open("/dev/audio", O_RDWR);
  if (fd == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, -1,
		      mus_format("can't open /dev/audio for output: %s",
				 strerror(errno)));
  ioctl(fd, AUDIO_SET_CHANNELS, chans);
  if (dev == MUS_AUDIO_SPEAKERS)
    ioctl(fd, AUDIO_SET_OUTPUT, AUDIO_OUT_SPEAKER);
  else
    if (dev == MUS_AUDIO_LINE_OUT)
      ioctl(fd, AUDIO_SET_OUTPUT, AUDIO_OUT_LINE);
    else ioctl(fd, AUDIO_SET_OUTPUT, AUDIO_OUT_HEADPHONE);
  if (format == MUS_BSHORT)
    ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_LINEAR16BIT);
  else
    if (format == MUS_MULAW)
      ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_ULAW);
  else 
    if (format == MUS_ALAW)
      ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_ALAW);
    else 
      RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, fd,
			mus_format("can't set output format to %d (%s) for %d (%s)",
				   format, mus_audio_format_name(format),
				   dev, 
				   mus_audio_device_name(dev)));
  ioctl(fd, AUDIO_DESCRIBE, &desc);
  for(i = 0; i < desc.nrates; i++) 
    if(srate == desc.sample_rate[i]) 
      break;
  if (i == desc.nrates) 
    RETURN_ERROR_EXIT(SRATE_NOT_AVAILABLE, fd,
		      mus_format("can't set srate to %d on %d (%s)",
				 srate, dev, 
				 mus_audio_device_name(dev)));
  ioctl(fd, AUDIO_SET_SAMPLE_RATE, srate);
  return(fd);
}

int mus_audio_write(int line, char *buf, int bytes)
{
  write(line, buf, bytes);
  return(MUS_NO_ERROR);
}

int mus_audio_close(int line) 
{
  close(line);
  return(MUS_NO_ERROR);
}

static void describe_audio_state_1(void)
{
  struct audio_describe desc;
  struct audio_gain gain;
  int mina, maxa, fd, tmp;
  int g[2];
  fd = open("/dev/audio", O_RDWR);
  if (fd == -1) return;
  ioctl(fd, AUDIO_GET_OUTPUT, &tmp);
  switch (tmp)
    {
    case AUDIO_OUT_SPEAKER:   pprint("output: speakers\n"); break;
    case AUDIO_OUT_HEADPHONE: pprint("output: headphone\n"); break;
    case AUDIO_OUT_LINE:      pprint("output: line out\n"); break;
    }
  ioctl(fd, AUDIO_GET_INPUT, &tmp);
  switch (tmp)
    {
    case AUDIO_IN_MIKE: pprint("input: mic\n"); break;
    case AUDIO_IN_LINE: pprint("input: line in\n"); break;
    }
  ioctl(fd, AUDIO_GET_DATA_FORMAT, &tmp);
  switch (tmp)
    {
    case AUDIO_FORMAT_LINEAR16BIT: pprint("format: 16-bit linear\n"); break;
    case AUDIO_FORMAT_ULAW:        pprint("format: mulaw\n"); break;
    case AUDIO_FORMAT_ALAW:        pprint("format: alaw\n"); break;
    }
  ioctl(fd, AUDIO_DESCRIBE, &desc);
  gain.channel_mask = (AUDIO_CHANNEL_LEFT | AUDIO_CHANNEL_RIGHT);
  ioctl(fd, AUDIO_GET_GAINS, &gain);
  close(fd);
  g[0] = gain.cgain[0].transmit_gain; 
  g[1] = gain.cgain[1].transmit_gain;
  mina = desc.min_transmit_gain;  
  maxa = desc.max_transmit_gain;
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "out vols: %.3f %.3f\n",
	  (float)(g[0] - mina) / (float)(maxa - mina),
	  (float)(g[1] - mina) / (float)(maxa - mina)); 
  pprint(audio_strbuf);
  g[0] = gain.cgain[0].receive_gain; 
  g[1] = gain.cgain[1].receive_gain;
  mina = desc.min_receive_gain;  
  maxa = desc.max_receive_gain;
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "in vols: %.3f %.3f\n",
	  (float)(g[0] - mina) / (float)(maxa - mina),
	  (float)(g[1] - mina) / (float)(maxa - mina)); 
  pprint(audio_strbuf);
  g[0] = gain.cgain[0].monitor_gain; 
  g[1] = gain.cgain[1].monitor_gain;
  mina = desc.min_monitor_gain;  
  maxa = desc.max_monitor_gain;
  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "monitor vols: %.3f %.3f\n",
	  (float)(g[0] - mina) / (float)(maxa - mina),
	  (float)(g[1] - mina) / (float)(maxa - mina)); 
  pprint(audio_strbuf);
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val) 
{
  struct audio_describe desc;
  struct audio_gain gain;
  int audio_fd = -1, srate, g, maxa, mina, dev, err = MUS_NO_ERROR;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  if (field == MUS_AUDIO_PORT)
    {
      val[0] = 4; 
      if (chan > 1) val[1] = MUS_AUDIO_MICROPHONE; 
      if (chan > 2) val[2] = MUS_AUDIO_DAC_OUT;
      if (chan > 3) val[3] = MUS_AUDIO_LINE_OUT;
      if (chan > 4) val[4] = MUS_AUDIO_LINE_IN;
    }
  else
    {
      if (field == FORMAT_FIELD)
        {
          val[0] = 3; 
          if (chan > 1) val[1] = MUS_BSHORT;
          if (chan > 2) val[2] = MUS_MULAW;
          if (chan > 3) val[3] = MUS_ALAW;
        }
      else
        {
          audio_fd = open("/dev/audio", O_RDWR);
          ioctl(audio_fd, AUDIO_DESCRIBE, &desc);
          switch (dev)
            {
            case MUS_AUDIO_DEFAULT:
            case MUS_AUDIO_DAC_OUT:
            case MUS_AUDIO_SPEAKERS:
            case MUS_AUDIO_LINE_OUT:
              switch (field)
                {
                case MUS_AUDIO_AMP: 
                  ioctl(audio_fd, AUDIO_GET_GAINS, &gain);
                  if (chan == 0) 
		    g = gain.cgain[0].transmit_gain; 
		  else g = gain.cgain[1].transmit_gain;
                  mina = desc.min_transmit_gain;  
		  maxa = desc.max_transmit_gain;
                  val[0] = (float)(g - mina) / (float)(maxa - mina);
                  break;
                case MUS_AUDIO_CHANNEL: 
		  val[0] = 2; 
		  break;
                case MUS_AUDIO_SRATE: 
                  ioctl(audio_fd, AUDIO_GET_SAMPLE_RATE, &srate); 
                  val[0] = srate; 
                  break;
                default: 
		  err = MUS_ERROR; 
		  break;
                }
              break;
            case MUS_AUDIO_MICROPHONE:
            case MUS_AUDIO_LINE_IN:
            case MUS_AUDIO_DUPLEX_DEFAULT:
              switch (field)
                {
                case MUS_AUDIO_AMP: 
                  ioctl(audio_fd, AUDIO_GET_GAINS, &gain);
                  if (chan == 0) 
		    g = gain.cgain[0].receive_gain; 
		  else g = gain.cgain[1].receive_gain;
                  mina = desc.min_receive_gain;  
		  maxa = desc.max_receive_gain;
                  val[0] = (float)(g - mina) / (float)(maxa - mina);
                  break;
                case MUS_AUDIO_CHANNEL: 
		  val[0] = 2; 
		  break;
                case MUS_AUDIO_SRATE: 
                  ioctl(audio_fd, AUDIO_GET_SAMPLE_RATE, &srate); 
                  val[0] = srate; 
                  break;
                default:
		  err = MUS_ERROR;
		  break;
                }
              break;
            default: 
	      err = MUS_ERROR;
	      break;
            }
        }
    }
  if (err == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't read %s field of device %d (%s)",
				 mus_audio_device_name(field),
				 dev, 
				 mus_audio_device_name(dev)));

  if (audio_fd != -1) close(audio_fd);
  return(MUS_NO_ERROR);
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val) 
{
  struct audio_describe desc;
  struct audio_gain gain;
  int audio_fd = -1, srate, g, maxa, mina, dev, err = MUS_NO_ERROR;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  audio_fd = open("/dev/audio", O_RDWR);
  ioctl(audio_fd, AUDIO_DESCRIBE, &desc);
  switch (dev)
    {
    case MUS_AUDIO_DEFAULT:
    case MUS_AUDIO_DAC_OUT:
    case MUS_AUDIO_SPEAKERS:
    case MUS_AUDIO_LINE_OUT:
      switch (field)
        {
        case MUS_AUDIO_AMP: 
          mina = desc.min_transmit_gain;  
	  maxa = desc.max_transmit_gain;
          ioctl(audio_fd, AUDIO_GET_GAINS, &gain);
          g = mina + val[0] * (maxa - mina);
          if (chan == 0) 
	    gain.cgain[0].transmit_gain = g; 
	  else gain.cgain[1].transmit_gain = g;
          ioctl(audio_fd, AUDIO_SET_GAINS, &gain);
          break;
        case MUS_AUDIO_SRATE: 
          srate = val[0];
          ioctl(audio_fd, AUDIO_SET_SAMPLE_RATE, srate); 
          break;
        default:
	  err = MUS_ERROR;
	  break;
        }
      break;
    case MUS_AUDIO_MICROPHONE:
    case MUS_AUDIO_LINE_IN:
    case MUS_AUDIO_DUPLEX_DEFAULT:
      switch (field)
        {
        case MUS_AUDIO_AMP: 
          mina = desc.min_receive_gain;  
	  maxa = desc.max_receive_gain;
          ioctl(audio_fd, AUDIO_GET_GAINS, &gain);
          g = mina + val[0] * (maxa - mina);
          if (chan == 0) 
	    gain.cgain[0].receive_gain = g; 
	  else gain.cgain[1].receive_gain = g;
          ioctl(audio_fd, AUDIO_SET_GAINS, &gain);
          break;
        case MUS_AUDIO_SRATE: 
          srate = val[0];
          ioctl(audio_fd, AUDIO_SET_SAMPLE_RATE, srate); 
          break;
        default:
	  err = MUS_ERROR;
	  break;
        }
      break;
    default:
      err = MUS_ERROR;
      break;
    }
  if (err == MUS_ERROR)
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't set %s field of device %d (%s)",
				 mus_audio_device_name(field),
				 dev, 
				 mus_audio_device_name(dev)));

  if (audio_fd != -1) close(audio_fd);
  return(MUS_NO_ERROR);
}

int mus_audio_initialize(void) {return(MUS_NO_ERROR);}

int mus_audio_systems(void) {return(1);}
char *mus_audio_system_name(int system) {return("HPUX");}

/* struct audio_status status_b;
 * ioctl(devAudio, AUDIO_GET_STATUS, &status_b)
 * not_busy = (status_b.transmit_status == AUDIO_DONE);
*/

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int size) 
{
  int fd, i, dev;
  struct audio_describe desc;
  dev = MUS_AUDIO_DEVICE(ur_dev);
  fd = open("/dev/audio", O_RDWR);
  if (fd == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, NULL,
		      mus_format("can't open /dev/audio for input: %s",
				 strerror(errno)));
  ioctl(fd, AUDIO_SET_CHANNELS, chans);
  if (dev == MUS_AUDIO_MICROPHONE)
    ioctl(fd, AUDIO_SET_INPUT, AUDIO_IN_MIKE);
  else ioctl(fd, AUDIO_SET_INPUT, AUDIO_IN_LINE);
  if (format == MUS_BSHORT)
    ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_LINEAR16BIT);
  else
    if (format == MUS_MULAW)
      ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_ULAW);
  else 
    if (format == MUS_ALAW)
      ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_ALAW);
    else 
      RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, fd,
			mus_format("can't set input format to %d (%s) on %d (%s)",
				   format, mus_audio_format_name(format),
				   dev, 
				   mus_audio_device_name(dev)));
  ioctl(fd, AUDIO_DESCRIBE, &desc);
  for(i = 0; i < desc.nrates; i++) 
    if(srate == desc.sample_rate[i]) 
      break;
  if (i == desc.nrates) 
    RETURN_ERROR_EXIT(MUS_AUDIO_SRATE_NOT_AVAILABLE, fd,
		      mus_format("can't set srate to %d on %d (%s)",
				 srate, dev, 
				 mus_audio_device_name(dev)));
  ioctl(fd, AUDIO_SET_SAMPLE_RATE, srate);
  return(fd);
}

int mus_audio_read(int line, char *buf, int bytes) 
{
  read(line, buf, bytes);
  return(MUS_NO_ERROR);
}

#endif



/* ------------------------------- NETBSD ----------------------------------------- */

#if defined(MUS_NETBSD) && (!(defined(AUDIO_OK)))
#define AUDIO_OK
/* started from Xanim a long time ago..., bugfixes from Thomas Klausner 30-Jul-05, worked into better shape Aug-05 */
#include <fcntl.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>

#define RETURN_ERROR_EXIT(Error_Type, Audio_Line, Ur_Error_Message) \
  do { char *Error_Message; Error_Message = Ur_Error_Message; \
    if (Audio_Line != -1) close(Audio_Line); \
    if (Error_Message) \
      {MUS_STANDARD_ERROR(Error_Type, Error_Message); FREE(Error_Message);} \
    else MUS_STANDARD_ERROR(Error_Type, mus_error_type_to_string(Error_Type)); \
    return(MUS_ERROR); \
  } while (false)

static int bsd_format_to_sndlib(int encoding)
{
  switch (encoding)
    {
    case AUDIO_ENCODING_ULAW:       return(MUS_MULAW);   break;
    case AUDIO_ENCODING_ALAW:       return(MUS_ALAW);    break;
    case AUDIO_ENCODING_LINEAR:     return(MUS_BSHORT);  break; /* "sun compatible" so probably big-endian? */
    case AUDIO_ENCODING_SLINEAR:
    case AUDIO_ENCODING_LINEAR8:    return(MUS_BYTE);    break;
    case AUDIO_ENCODING_SLINEAR_LE: return(MUS_LSHORT);  break;
    case AUDIO_ENCODING_SLINEAR_BE: return(MUS_BSHORT);  break;
    case AUDIO_ENCODING_ULINEAR_LE: return(MUS_ULSHORT); break;
    case AUDIO_ENCODING_ULINEAR_BE: return(MUS_UBSHORT); break;
    case AUDIO_ENCODING_ULINEAR:    return(MUS_UBYTE);   break;
    case AUDIO_ENCODING_NONE:
    case AUDIO_ENCODING_ADPCM: 
    default:                        return(MUS_UNKNOWN); break;
    }
  return(MUS_UNKNOWN);
}

static int sndlib_format_to_bsd(int encoding)
{
  switch (encoding)
    {
    case MUS_MULAW:   return(AUDIO_ENCODING_ULAW);       break;
    case MUS_ALAW:    return(AUDIO_ENCODING_ALAW);       break;
    case MUS_BYTE:    return(AUDIO_ENCODING_SLINEAR);    break;
    case MUS_LSHORT:  return(AUDIO_ENCODING_SLINEAR_LE); break;
    case MUS_BSHORT:  return(AUDIO_ENCODING_SLINEAR_BE); break;
    case MUS_ULSHORT: return(AUDIO_ENCODING_ULINEAR_LE); break;
    case MUS_UBSHORT: return(AUDIO_ENCODING_ULINEAR_BE); break;
    case MUS_UBYTE:   return(AUDIO_ENCODING_ULINEAR);    break;
    }
  return(AUDIO_ENCODING_NONE);
}

int mus_audio_initialize(void) 
{
  return(MUS_NO_ERROR);
}

int mus_audio_systems(void) 
{
  return(1);
}

char *mus_audio_system_name(int system) 
{
  return("NetBSD");
}

char *mus_audio_moniker(void) 
{
  return("NetBSD audio");
}

static int cur_chans = 1, cur_srate = 22050;

int mus_audio_write(int line, char *buf, int bytes) 
{
  /* trouble... AUDIO_WSEEK always returns 0, no way to tell that I'm about to
   *   hit "hiwat", but when I do, it hangs.  Can't use AUDIO_DRAIN --
   *   it introduces interruptions.  Not sure what to do...
   */
  int b = 0;
  b = write(line, buf, bytes);
  usleep(10000);
  if ((b != bytes) && (b > 0)) /* b <= 0 presumably some sort of error, and we want to avoid infinite recursion below */
    {
      /* hangs at close if we don't handle this somehow */
      if ((cur_chans == 1) || (cur_srate == 22050))
	sleep(1);
      else usleep(10000);
      mus_audio_write(line, (char *)(buf + b), bytes - b);
    }
  return(MUS_NO_ERROR);
}

int mus_audio_close(int line) 
{
  usleep(100000);
  ioctl(line, AUDIO_FLUSH, 0);
  close(line);
  return(MUS_NO_ERROR);
}

static int netbsd_default_outputs = (AUDIO_HEADPHONE | AUDIO_LINE_OUT | AUDIO_SPEAKER);

void mus_netbsd_set_outputs(int speakers, int headphones, int line_out)
{
  netbsd_default_outputs = 0;
  if (speakers) netbsd_default_outputs |= AUDIO_SPEAKER;
  if (headphones) netbsd_default_outputs |= AUDIO_HEADPHONE;
  if (line_out) netbsd_default_outputs |= AUDIO_LINE_OUT;
}

int mus_audio_open_output(int dev, int srate, int chans, int format, int size) 
{
  int line, encode;
  audio_info_t a_info;

  line = open("/dev/sound", O_WRONLY | O_NDELAY); /* /dev/audio assumes mono 8-bit mulaw */
  if (line == -1)
    {
      if (errno == EBUSY) 
	return(mus_error(MUS_AUDIO_CANT_OPEN, NULL));
      else return(mus_error(MUS_AUDIO_DEVICE_NOT_AVAILABLE, NULL));
    }
  AUDIO_INITINFO(&a_info);

  /* a_info.blocksize = size; */
  encode = sndlib_format_to_bsd(format);
  if (encode == AUDIO_ENCODING_NONE)
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %d (%s) not available",
				 format, 
				 mus_data_format_name(format)));
  a_info.play.encoding = encode;
  a_info.mode = AUMODE_PLAY | AUMODE_PLAY_ALL;
  a_info.play.precision = mus_bytes_per_sample(format) * 8;
  a_info.play.sample_rate = srate;
  if (dev == MUS_AUDIO_LINE_OUT)
    a_info.play.port = AUDIO_LINE_OUT;
  else
    {
      if (dev == MUS_AUDIO_SPEAKERS)
	a_info.play.port = AUDIO_SPEAKER | (netbsd_default_outputs & AUDIO_HEADPHONE);
      else a_info.play.port = netbsd_default_outputs;
    }
  a_info.play.channels = chans;
  ioctl(line, AUDIO_SETINFO, &a_info);
  /* actually doesn't set the "ports" field -- always 0 */

  ioctl(line, AUDIO_GETINFO, &a_info);

  if ((int)(a_info.play.sample_rate) != srate)
    mus_print("srate: %d -> %d\n", srate, a_info.play.sample_rate);
  if ((int)(a_info.play.encoding) != sndlib_format_to_bsd(format))
    mus_print("encoding: %d -> %d\n", sndlib_format_to_bsd(format), a_info.play.encoding);
  if ((int)(a_info.play.channels) != chans)
    mus_print("chans: %d -> %d\n", chans, a_info.play.channels);

  cur_chans = chans;
  cur_srate = srate;

  return(line);
}

int mus_audio_read(int line, char *buf, int bytes) 
{
  read(line, buf, bytes);
  return(MUS_NO_ERROR);
}

static void describe_audio_state_1(void) 
{
  audio_device_t dev;
  int i = 0, val, err = 0;
  int line;
  float amp;
  audio_info_t a_info;
  audio_encoding_t e_info;

  pprint("NetBSD ");
  line = open("/dev/sound", O_WRONLY | O_NDELAY);
  if (line == -1)
    return;

  pprint("/dev/sound:\n");
  err = ioctl(line, AUDIO_GETDEV, &dev);
  if (err == 0)
    {
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "%s: version: %s (%s)", dev.name, dev.version, dev.config);
      pprint(audio_strbuf);
    }

  err = ioctl(line, AUDIO_GETPROPS, &val);
  if (err == 0)
    {
      if (val & AUDIO_PROP_FULLDUPLEX) 
	pprint(" full-duplex"); 
      else pprint(" half-duplex");
    }
  pprint("\n");

  err = ioctl(line, AUDIO_GETINFO, &a_info);
  if (err == 0)
    {
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  play: srate: %d, chans: %d, format: %s (%d bits), ",
		   a_info.play.sample_rate, 
		   a_info.play.channels, 
		   mus_data_format_short_name(bsd_format_to_sndlib(a_info.play.encoding)), 
		   a_info.play.precision);
      pprint(audio_strbuf);

      amp = (float)(a_info.play.gain - AUDIO_MIN_GAIN) / (float)(AUDIO_MAX_GAIN - AUDIO_MIN_GAIN);
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "volume: %.3f %.3f (gain: %d, balance: %d)\n",
		   amp * (1.0 - ((float)(a_info.play.balance) / (float)(2 * AUDIO_MID_BALANCE))),
		   amp * ((float)(a_info.play.balance) / (float)(2 * AUDIO_MID_BALANCE)),
		   a_info.play.gain, a_info.play.balance);
      pprint(audio_strbuf);

      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  record: srate: %d, chans: %d, format: %s (%d bits), ",
		   a_info.record.sample_rate, 
		   a_info.record.channels, 
		   mus_data_format_short_name(bsd_format_to_sndlib(a_info.record.encoding)), 
		   a_info.record.precision);
      pprint(audio_strbuf);

      amp = (float)(a_info.record.gain - AUDIO_MIN_GAIN) / (float)(AUDIO_MAX_GAIN - AUDIO_MIN_GAIN);
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "volume: %.3f %.3f (gain: %d, balance: %d)\n",
		   amp * (1.0 - ((float)(a_info.record.balance) / (float)(2 * AUDIO_MID_BALANCE))),
		   amp * ((float)(a_info.record.balance) / (float)(2 * AUDIO_MID_BALANCE)),
		   a_info.record.gain, a_info.record.balance);
      pprint(audio_strbuf);
    }

  mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "  available encodings:\n");
  pprint(audio_strbuf);

  for (i = 0; ; i++)
    {
      e_info.index = i;
      err = ioctl(line, AUDIO_GETENC, &e_info);
      if (err != 0) break;
      mus_snprintf(audio_strbuf, PRINT_BUFFER_SIZE, "    %s (%s, bits: %d)\n",
		   mus_data_format_short_name(bsd_format_to_sndlib(e_info.encoding)),
		   e_info.name, 
		   e_info.precision);
      pprint(audio_strbuf);
    }

  close(line);

#if 0
  /* I don't see anything useful in all this mixer data, so I'll omit it */
  fprintf(stderr,"/dev/mixer:\n");
  line = open("/dev/mixer", O_RDONLY | O_NDELAY);
  if (line == -1)
    return;
  val = ioctl(line, AUDIO_GETDEV, &dev);
  fprintf(stderr, "\n%d, name: %s, version: %s, config: %s\n",
	  val, dev.name, dev.version, dev.config);
  for (i = 0; ; i++)
    {
      mdev.index = i;
      val = ioctl(line, AUDIO_MIXER_DEVINFO, &mdev);
      if (val != 0) break;
      fprintf(stderr,"%d: name: %s ", i, mdev.label.name);
      fprintf(stderr,"class: %d, type: %d, units: %s, chans: %d, delta: %d\n", 
	      mdev.mixer_class, mdev.type, mdev.un.v.units.name, mdev.un.v.num_channels, mdev.un.v.delta);
      mx.dev = i;
      ioctl(line, AUDIO_MIXER_READ, &mx);
      switch (mx.type)
	{
	case AUDIO_MIXER_CLASS:
	  fprintf(stderr, "mixer read: class type?\n"); 
	  break;
	case AUDIO_MIXER_ENUM:
	  fprintf(stderr, "mixer read: enum: %d\n", mx.un.ord);
	  break;
	case AUDIO_MIXER_SET:
	case AUDIO_MIXER_VALUE:
	  {
	    int j;
	    ml = mx.un.value;
	    fprintf(stderr, "mixer read: level: %d chans [", ml.num_channels);
	    for (j = 0; j < ml.num_channels; j++)
	      fprintf(stderr, "%d ", (int)(ml.level[j]));
	    fprintf(stderr, "]\n");
	  }
	  break;
	default:
	  fprintf(stderr, "mixer read: unknown type? %d\n", mx.type); 
	  break;
	}
    }
#endif
}

int mus_audio_mixer_read(int ur_dev, int field, int chan, float *val) 
{
  int i, audio_fd, err, dev;
  audio_info_t info;
  bool ok = true;

  dev = MUS_AUDIO_DEVICE(ur_dev);
  AUDIO_INITINFO(&info);
  audio_fd = open("/dev/sound", O_RDONLY | O_NONBLOCK, 0);
  if (audio_fd == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, -1,
		      mus_format("can't open /dev/sound: %s",
				 strerror(errno)));
  err = ioctl(audio_fd, AUDIO_GETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't get dac info"));

  if (field == MUS_AUDIO_PORT)
    {
      val[0] = 1;
      val[1] = MUS_AUDIO_MICROPHONE; 
    }
  else
    {
      if (field == MUS_AUDIO_FORMAT)
        {
	  audio_encoding_t e_info;
	  for (i = 0; ; i++)
	    {
	      e_info.index = i;
	      err = ioctl(audio_fd, AUDIO_GETENC, &e_info);
	      if (err != 0) break;
	      val[i + 1] = bsd_format_to_sndlib(e_info.encoding);
	    }
	  val[0] = i;
        }
      else
        {
          switch (dev)
            {
            case MUS_AUDIO_DEFAULT:
            case MUS_AUDIO_DAC_OUT:
            case MUS_AUDIO_SPEAKERS:
            case MUS_AUDIO_LINE_OUT:
              switch (field)
                {
                case MUS_AUDIO_AMP: 
		  {
		    float amp;
		    amp = (float)(info.play.gain - AUDIO_MIN_GAIN) / (float)(AUDIO_MAX_GAIN - AUDIO_MIN_GAIN);
		    if (chan == 0)
		      val[0] = amp * (1.0 - ((float)(info.play.balance) / (float)(2 * AUDIO_MID_BALANCE)));
		    else val[0] = amp * ((float)(info.play.balance) / (float)(2 * AUDIO_MID_BALANCE));
		  }
		  break;
                case MUS_AUDIO_CHANNEL: 
		  val[0] = 2; 
		  break;
                case MUS_AUDIO_SRATE: 
		  val[0] = (float)info.play.sample_rate; 
		  break;
                default: 
		  ok = false;
		  break;
                }
              break;
            case MUS_AUDIO_MICROPHONE:
            case MUS_AUDIO_LINE_IN:
            case MUS_AUDIO_DUPLEX_DEFAULT:
	    case MUS_AUDIO_CD:
              switch (field)
                {
                case MUS_AUDIO_AMP:
		  {
		    float amp;
		    amp = (float)(info.record.gain - AUDIO_MIN_GAIN) / (float)(AUDIO_MAX_GAIN - AUDIO_MIN_GAIN);
		    if (chan == 0)
		      val[0] = amp * (1.0 - ((float)(info.record.balance) / (float)(2 * AUDIO_MID_BALANCE)));
		    else val[0] = amp * ((float)(info.record.balance) / (float)(2 * AUDIO_MID_BALANCE));
		  }
		  break;
                case MUS_AUDIO_CHANNEL: 
		  val[0] = 1; 
		  break;
                case MUS_AUDIO_SRATE: 
		  val[0] = (float)(info.record.sample_rate); 
		  break;
                default: 
		  ok = false;
		  break;
                }
              break;
            default: 
	      ok = false;
	      break;
            }
        }
    }
  if (!ok)
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd, 
		      mus_format("can't read %s field %d (%s)",
				 mus_audio_device_name(dev),
				 field, 
				 mus_audio_device_name(field)));
  return(mus_audio_close(audio_fd));
}

int mus_audio_mixer_write(int ur_dev, int field, int chan, float *val) 
{
  audio_info_t info;
  int dev, audio_fd, err;
  bool ok = true;

  dev = MUS_AUDIO_DEVICE(ur_dev);
  AUDIO_INITINFO(&info);

  audio_fd = open("/dev/sound", O_RDWR | O_NONBLOCK, 0);
  if (audio_fd == -1)
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, -1,
		      mus_format("can't open /dev/sound: %s",
				 strerror(errno)));
    
  err = ioctl(audio_fd, AUDIO_GETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_READ, audio_fd,
		      mus_format("can't get /dev/sound info"));

  switch (dev)
    {
    case MUS_AUDIO_DEFAULT:
    case MUS_AUDIO_DAC_OUT:
    case MUS_AUDIO_SPEAKERS:
    case MUS_AUDIO_LINE_OUT:
      switch (field)
        {
        case MUS_AUDIO_AMP: 
	  info.play.balance = AUDIO_MID_BALANCE;
	  info.play.gain = AUDIO_MIN_GAIN + (int)((AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * val[0]);
	  break;
        case MUS_AUDIO_CHANNEL: 
	  info.play.channels = (int)val[0]; 
	  break;
        case MUS_AUDIO_SRATE: 
	  info.play.sample_rate = (int)val[0]; 
	  break;
        default: 
	  ok = false;
	  break;
        }
      break;
    case MUS_AUDIO_MICROPHONE:
      switch (field)
	{
        case MUS_AUDIO_AMP: 
	  info.record.gain = AUDIO_MIN_GAIN + (int)((AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * val[0]);
	  info.record.balance = AUDIO_MID_BALANCE;
	  break;
        case MUS_AUDIO_CHANNEL: 
	  info.record.channels = (int)val[0];
	  break;
        case MUS_AUDIO_SRATE: 
	  info.record.sample_rate = (int)val[0];
	  break;
        default: 
	  ok = false;
	  break;
	}
      break;
    case MUS_AUDIO_LINE_IN:
    case MUS_AUDIO_DUPLEX_DEFAULT:
    case MUS_AUDIO_CD:
      switch (field)
        {
        case MUS_AUDIO_AMP: 
	  info.record.balance = AUDIO_MID_BALANCE;
	  info.record.gain = AUDIO_MIN_GAIN + (int)((AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * val[0]);
	  break;
        case MUS_AUDIO_CHANNEL: 
	  info.record.channels = (int)val[0]; 
	  break;
        case MUS_AUDIO_SRATE: 
	  info.record.sample_rate = (int)val[0]; 
	  break;
        default: 
	  ok = false;
	  break;
        }
      break;
    default: 
      ok = false;
      break;
    }
  if (ok)
    ioctl(audio_fd, AUDIO_SETINFO, &info); 
  else
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
		      mus_format("can't write %s field %d (%s)",
				 mus_audio_device_name(dev),
				 field, 
				 mus_audio_device_name(field)));
  return(mus_audio_close(audio_fd));
}

int mus_audio_open_input(int ur_dev, int srate, int chans, int format, int size) 
{
  audio_info_t info;
  int encode, bits, dev, audio_fd, err;

  dev = MUS_AUDIO_DEVICE(ur_dev);
  encode = sndlib_format_to_bsd(format);
  bits = 8 * mus_bytes_per_sample(format);
  if (encode == AUDIO_ENCODING_NONE) 
    RETURN_ERROR_EXIT(MUS_AUDIO_FORMAT_NOT_AVAILABLE, -1,
		      mus_format("format %s not available for recording",
				 mus_data_format_name(format)));

  if (dev != MUS_AUDIO_DUPLEX_DEFAULT)
    audio_fd = open("/dev/sound", O_RDONLY, 0);
  else audio_fd = open("/dev/sound", O_RDWR, 0);
  if (audio_fd == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_OPEN, -1,
		      mus_format("can't open /dev/sound: %s",
				 strerror(errno)));

  AUDIO_INITINFO(&info);
  info.record.sample_rate = srate;
  info.record.channels = chans;
  info.record.precision = bits;
  info.record.encoding = encode;
  info.record.port = AUDIO_MICROPHONE;
  err = ioctl(audio_fd, AUDIO_SETINFO, &info); 
  if (err == -1) 
    RETURN_ERROR_EXIT(MUS_AUDIO_CANT_WRITE, audio_fd,
		      mus_format("can't set up for recording"));
  return(audio_fd);
}

#endif



/* ------------------------------- STUBS ----------------------------------------- */

#ifndef AUDIO_OK
static void describe_audio_state_1(void) {pprint("audio stubbed out");}
int mus_audio_open_output(int dev, int srate, int chans, int format, int size) {return(MUS_ERROR);}
int mus_audio_open_input(int dev, int srate, int chans, int format, int size) {return(MUS_ERROR);}
int mus_audio_write(int line, char *buf, int bytes) {return(MUS_ERROR);}
int mus_audio_close(int line) {return(MUS_ERROR);}
int mus_audio_read(int line, char *buf, int bytes) {return(MUS_ERROR);}
int mus_audio_mixer_read(int dev, int field, int chan, float *val) {return(MUS_ERROR);}
int mus_audio_mixer_write(int dev, int field, int chan, float *val) {return(MUS_ERROR);}
int mus_audio_initialize(void) {return(MUS_ERROR);}
int mus_audio_systems(void) {return(0);}
char *mus_audio_system_name(int system) {return("none");}
char *mus_audio_moniker(void) {return("no audio support");}
#endif



static char *save_it = NULL;
static int print_it = 1;
static int save_it_len = 0;
static int save_it_loc = 0;

static void pprint(char *str)
{
  int i, len;
  if ((str) && (*str))
    {
      if ((print_it) || (!(save_it)))
	{
	  mus_print(str);
	}
      else
	{
	  len = strlen(str);
	  if ((len + save_it_loc + 2) >= save_it_len)
	    {
	      save_it_len = (len + save_it_loc + 1024);
	      save_it = (char *)REALLOC(save_it, save_it_len * sizeof(char));
	    }
	  for (i = 0; i < len; i++)
	    save_it[save_it_loc++] = str[i];
	  save_it[save_it_loc] = 0;
	}
    }
}

char *mus_audio_report(void)
{
  mus_audio_initialize();
  if (!(save_it)) 
    {
      save_it_len = 1024;
      save_it = (char *)CALLOC(save_it_len, sizeof(char));
    }
  save_it_loc = 0;
  print_it = 0;
  if (!audio_strbuf) audio_strbuf = (char *)CALLOC(PRINT_BUFFER_SIZE, sizeof(char));
  describe_audio_state_1();
  return(save_it);
}

void mus_audio_describe(void)
{
  mus_audio_initialize();
  print_it = 1;
  if (!audio_strbuf) audio_strbuf = (char *)CALLOC(PRINT_BUFFER_SIZE, sizeof(char));
  describe_audio_state_1();
}

/* for CLM */
void mus_reset_audio_c(void)
{
  audio_initialized = false;
  save_it = NULL;
  version_name = NULL;
#ifdef MUS_SUN
  sun_vol_name = NULL;
#endif
  save_it_len = 0;
  audio_strbuf = NULL;
}


int mus_audio_compatible_format(int dev) 
{
#if HAVE_ALSA || HAVE_JACK
  int err, i;
  float val[32];
  int ival[32];
  err = mus_audio_mixer_read(dev, MUS_AUDIO_FORMAT, 32, val);
  if (err != MUS_ERROR)
    {
      for (i = 0; i <= (int)(val[0]); i++) ival[i] = (int)(val[i]);
      /*               ^ this cast is vital!  Memory clobbered otherwise in LinuxPPC */
      for (i = 1; i <= ival[0]; i++)
	if (ival[i] == MUS_AUDIO_COMPATIBLE_FORMAT) 
	  return(MUS_AUDIO_COMPATIBLE_FORMAT);
      for (i = 1; i <= ival[0]; i++) 
	if ((ival[i] == MUS_BINT) || (ival[i] == MUS_LINT) ||
	    (ival[i] == MUS_BFLOAT) || (ival[i] == MUS_LFLOAT) ||
	    (ival[i] == MUS_BSHORT) || (ival[i] == MUS_LSHORT))
	  return(ival[i]);
      for (i = 1; i <= ival[0]; i++) 
	if ((ival[i] == MUS_MULAW) || (ival[i] == MUS_ALAW) ||
	    (ival[i] == MUS_UBYTE) || (ival[i] == MUS_BYTE))
	  return(ival[i]);
      return(ival[1]);
    }
#endif
  return(MUS_AUDIO_COMPATIBLE_FORMAT);
}


/* next two added 17-Dec-02 for non-interleaved audio IO */
static char *output_buffer = NULL;
static int output_buffer_size = 0;

int mus_audio_write_buffers(int port, int frames, int chans, mus_sample_t **bufs, int output_format, bool clipped)
{
  int bytes;
  bytes = chans * frames * mus_bytes_per_sample(output_format);
  if (output_buffer_size < bytes)
    {
      if (output_buffer) free(output_buffer);
      output_buffer = (char *)malloc(bytes);
      output_buffer_size = bytes;
    }
  mus_file_write_buffer(output_format, 0, frames - 1, chans, bufs, output_buffer, clipped);
  return(mus_audio_write(port, output_buffer, bytes));
}

static char *input_buffer = NULL;
static int input_buffer_size = 0;

int mus_audio_read_buffers(int port, int frames, int chans, mus_sample_t **bufs, int input_format)
{
  int bytes;
  bytes = chans * frames * mus_bytes_per_sample(input_format);
  if (input_buffer_size < bytes)
    {
      if (input_buffer) free(input_buffer);
      input_buffer = (char *)malloc(bytes);
      input_buffer_size = bytes;
    }
  mus_audio_read(port, input_buffer, bytes);
  return(mus_file_read_buffer(input_format, 0, chans, frames, bufs, input_buffer));
}
