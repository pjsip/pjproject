#ifndef SNDLIB_H
#define SNDLIB_H

#define SNDLIB_VERSION 20
#define SNDLIB_REVISION 0
#define SNDLIB_DATE "28-Mar-06"

#include <stdio.h>
#include <mus-config.h>

#if HAVE_UNISTD_H && (!(defined(_MSC_VER)))
  #include <unistd.h>
#endif

#include <sys/types.h>

#ifndef __cplusplus
#if HAVE_STDBOOL_H
  #include <stdbool.h>
#else
#ifndef true
  #define bool	int
  #define true	1
  #define false	0
#endif
#endif
#endif

#if (defined(SIZEOF_OFF_T) && (SIZEOF_OFF_T > 4)) || (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64))
  #if (SIZEOF_OFF_T == SIZEOF_LONG)
    #define OFF_TD "%ld"
  #else
    #define OFF_TD "%lld"
  #endif
#else
  #define OFF_TD "%d"
#endif

#ifndef MUS_LITTLE_ENDIAN
  #if WORDS_BIGENDIAN
    #define MUS_LITTLE_ENDIAN 0
  #else
    #define MUS_LITTLE_ENDIAN 1
  #endif
#endif

#ifndef c__FUNCTION__
#if (HAVE___FUNC__) || (defined(__STDC__) && defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))
  #define c__FUNCTION__ __func__
#else
#ifdef __GNUC__
  #define c__FUNCTION__ __FUNCTION__
#else
  #define c__FUNCTION__ ""
#endif
#endif
#endif

#if (!defined(M_PI))
  #define M_PI 3.14159265358979323846264338327
  #define M_PI_2 (M_PI / 2.0)
#endif

#define POWER_OF_2_P(x)	((((x) - 1) & (x)) == 0)
/* from sys/param.h */

#ifndef SEEK_SET
  #define SEEK_SET 0
  #define SEEK_END 2
#endif

#if (!SNDLIB_USE_FLOATS)
  #define mus_sample_t int
  #ifndef MUS_SAMPLE_BITS
    #define MUS_SAMPLE_BITS 24
  #endif
  #define MUS_SAMPLE_0 0
  #define MUS_BYTE_TO_SAMPLE(n) ((mus_sample_t)((n) << (MUS_SAMPLE_BITS - 8)))
  #define MUS_SAMPLE_TO_BYTE(n) ((n) >> (MUS_SAMPLE_BITS - 8))
  #define MUS_SHORT_TO_SAMPLE(n) ((mus_sample_t)((n) << (MUS_SAMPLE_BITS - 16)))
  #define MUS_SAMPLE_TO_SHORT(n) ((short)((n) >> (MUS_SAMPLE_BITS - 16)))
  #if (MUS_SAMPLE_BITS < 24)
    #define MUS_INT24_TO_SAMPLE(n) ((mus_sample_t)((n) >> (24 - MUS_SAMPLE_BITS)))
    #define MUS_SAMPLE_TO_INT24(n) ((int)((n) << (24 - MUS_SAMPLE_BITS)))
  #else
    #define MUS_INT24_TO_SAMPLE(n) ((mus_sample_t)((n) << (MUS_SAMPLE_BITS - 24)))
    #define MUS_SAMPLE_TO_INT24(n) ((int)((n) >> (MUS_SAMPLE_BITS - 24)))
  #endif
  #define MUS_INT_TO_SAMPLE(n) ((mus_sample_t)(n))
  #define MUS_SAMPLE_TO_INT(n) ((int)(n))
  /* these are for direct read/write (no cross-image assumption is made about 32 bit int scaling) */
  #define MUS_FLOAT_TO_FIX ((MUS_SAMPLE_BITS < 32) ? (1 << (MUS_SAMPLE_BITS - 1)) : 0x7fffffff)
  #define MUS_FIX_TO_FLOAT (1.0 / (float)(MUS_FLOAT_TO_FIX))
  #define MUS_FLOAT_TO_SAMPLE(n) ((mus_sample_t)((n) * MUS_FLOAT_TO_FIX))
  #define MUS_SAMPLE_TO_FLOAT(n) ((float)((n) * MUS_FIX_TO_FLOAT))
  #define MUS_DOUBLE_TO_SAMPLE(n) ((mus_sample_t)((n) * MUS_FLOAT_TO_FIX))
  #define MUS_SAMPLE_TO_DOUBLE(n) ((double)((n) * MUS_FIX_TO_FLOAT))
  #define MUS_SAMPLE_MAX ((mus_sample_t)((MUS_SAMPLE_BITS < 32) ? (MUS_FLOAT_TO_FIX - 1) : 0x7fffffff))
  #define MUS_SAMPLE_MIN ((mus_sample_t)((MUS_SAMPLE_BITS < 32) ? (-(MUS_FLOAT_TO_FIX)) : -0x7fffffff))
  #define mus_sample_abs(Sample) abs(Sample)
#else
  #define mus_sample_t Float
  #ifndef MUS_SAMPLE_BITS
    #define MUS_SAMPLE_BITS 24
  #endif
  #define MUS_SAMPLE_0 0.0
  #define MUS_BYTE_TO_SAMPLE(n) ((mus_sample_t)((Float)(n) / (Float)(1 << 7)))
  #define MUS_SHORT_TO_SAMPLE(n) ((mus_sample_t)((Float)(n) / (Float)(1 << 15)))
  #define MUS_INT_TO_SAMPLE(n) ((mus_sample_t)((Float)(n) / (Float)(1 << (MUS_SAMPLE_BITS - 1))))
  #define MUS_INT24_TO_SAMPLE(n) ((mus_sample_t)((Float)(n) / (Float)(1 << 23)))
  #define MUS_FLOAT_TO_FIX 1.0
  #define MUS_FIX_TO_FLOAT 1.0
  #define MUS_FLOAT_TO_SAMPLE(n) ((mus_sample_t)(n))
  #define MUS_DOUBLE_TO_SAMPLE(n) ((mus_sample_t)(n))
  #define MUS_SAMPLE_TO_FLOAT(n) ((Float)(n))
  #define MUS_SAMPLE_TO_DOUBLE(n) ((double)(n))
  #define MUS_SAMPLE_TO_INT(n) ((int)((n) * (1 << (MUS_SAMPLE_BITS - 1))))
  #define MUS_SAMPLE_TO_INT24(n) ((int)((n) * (1 << 23)))
  #define MUS_SAMPLE_TO_SHORT(n) ((short)((n) * (1 << 15)))
  #define MUS_SAMPLE_TO_BYTE(n) ((char)((n) * (1 << 7)))
  #define MUS_SAMPLE_MAX 0.99999
  #define MUS_SAMPLE_MIN (-1.0)
  #define mus_sample_abs(Sample) fabs(Sample)
#endif

enum {MUS_UNSUPPORTED, MUS_NEXT, MUS_AIFC, MUS_RIFF, MUS_BICSF, MUS_NIST, MUS_INRS, MUS_ESPS, MUS_SVX, MUS_VOC, 
      MUS_SNDT, MUS_RAW, MUS_SMP, MUS_AVR, MUS_IRCAM, MUS_SD1, MUS_SPPACK, MUS_MUS10, MUS_HCOM, MUS_PSION, MUS_MAUD,
      MUS_IEEE, MUS_MATLAB, MUS_ADC, MUS_MIDI, MUS_SOUNDFONT, MUS_GRAVIS, MUS_COMDISCO, MUS_GOLDWAVE, MUS_SRFS,
      MUS_MIDI_SAMPLE_DUMP, MUS_DIAMONDWARE, MUS_ADF, MUS_SBSTUDIOII, MUS_DELUSION,
      MUS_FARANDOLE, MUS_SAMPLE_DUMP, MUS_ULTRATRACKER, MUS_YAMAHA_SY85, MUS_YAMAHA_TX16W, MUS_DIGIPLAYER,
      MUS_COVOX, MUS_AVI, MUS_OMF, MUS_QUICKTIME, MUS_ASF, MUS_YAMAHA_SY99, MUS_KURZWEIL_2000,
      MUS_AIFF, MUS_PAF, MUS_CSL, MUS_FILE_SAMP, MUS_PVF, MUS_SOUNDFORGE, MUS_TWINVQ, MUS_AKAI4,
      MUS_IMPULSETRACKER, MUS_KORG, MUS_NVF, MUS_MAUI, MUS_SDIF, MUS_OGG, MUS_FLAC, MUS_SPEEX, MUS_MPEG,
      MUS_SHORTEN, MUS_TTA, MUS_WAVPACK,
      MUS_NUM_HEADER_TYPES};

#if defined(__GNUC__) && (!(defined(__cplusplus)))
  #define MUS_HEADER_TYPE_OK(n) ({ int _sndlib_h_0 = n; ((_sndlib_h_0 > MUS_UNSUPPORTED) && (_sndlib_h_0 <= MUS_MAUI)); })
#else
  #define MUS_HEADER_TYPE_OK(n) (((n) > MUS_UNSUPPORTED) && ((n) <= MUS_MAUI))
#endif

enum {MUS_UNKNOWN, MUS_BSHORT, MUS_MULAW, MUS_BYTE, MUS_BFLOAT, MUS_BINT, MUS_ALAW, MUS_UBYTE, MUS_B24INT,
      MUS_BDOUBLE, MUS_LSHORT, MUS_LINT, MUS_LFLOAT, MUS_LDOUBLE, MUS_UBSHORT, MUS_ULSHORT, MUS_L24INT,
      MUS_BINTN, MUS_LINTN, MUS_BFLOAT_UNSCALED, MUS_LFLOAT_UNSCALED, MUS_BDOUBLE_UNSCALED, MUS_LDOUBLE_UNSCALED,
      MUS_NUM_DATA_FORMATS};

/* MUS_LINTN and MUS_BINTN refer to 32 bit ints with 31 bits of "fraction" -- the data is "left justified" */
/* "unscaled" means the float value is used directly (i.e. not as -1.0 to 1.0, but (probably) -32768.0 to 32768.0) */

#if defined(__GNUC__) && (!(defined(__cplusplus)))
  #define MUS_DATA_FORMAT_OK(n) ({ int _sndlib_h_1 = n; ((_sndlib_h_1 > MUS_UNKNOWN) && (_sndlib_h_1 < MUS_NUM_DATA_FORMATS)); })
#else
  #define MUS_DATA_FORMAT_OK(n) (((n) > MUS_UNKNOWN) && ((n) < MUS_NUM_DATA_FORMATS))
#endif

#if MUS_MAC_OSX
  #if MUS_LITTLE_ENDIAN
    #define MUS_AUDIO_COMPATIBLE_FORMAT MUS_LFLOAT
  #else
    #define MUS_AUDIO_COMPATIBLE_FORMAT MUS_BFLOAT
  #endif
#else
  #if MUS_LITTLE_ENDIAN
    #define MUS_AUDIO_COMPATIBLE_FORMAT MUS_LSHORT
  #else
    #define MUS_AUDIO_COMPATIBLE_FORMAT MUS_BSHORT
  #endif
#endif

#define MUS_NIST_SHORTPACK 2
#define MUS_AIFF_IMA_ADPCM 99

#define MUS_AUDIO_PACK_SYSTEM(n) ((n) << 16)
#define MUS_AUDIO_SYSTEM(n) (((n) >> 16) & 0xffff)
#define MUS_AUDIO_DEVICE(n) ((n) & 0xffff)

enum {MUS_AUDIO_DEFAULT, MUS_AUDIO_DUPLEX_DEFAULT, MUS_AUDIO_ADAT_IN, MUS_AUDIO_AES_IN, MUS_AUDIO_LINE_OUT,
      MUS_AUDIO_LINE_IN, MUS_AUDIO_MICROPHONE, MUS_AUDIO_SPEAKERS, MUS_AUDIO_DIGITAL_IN, MUS_AUDIO_DIGITAL_OUT,
      MUS_AUDIO_DAC_OUT, MUS_AUDIO_ADAT_OUT, MUS_AUDIO_AES_OUT, MUS_AUDIO_DAC_FILTER, MUS_AUDIO_MIXER,
      MUS_AUDIO_LINE1, MUS_AUDIO_LINE2, MUS_AUDIO_LINE3, MUS_AUDIO_AUX_INPUT, MUS_AUDIO_CD,
      MUS_AUDIO_AUX_OUTPUT, MUS_AUDIO_SPDIF_IN, MUS_AUDIO_SPDIF_OUT, MUS_AUDIO_AMP, MUS_AUDIO_SRATE,
      MUS_AUDIO_CHANNEL, MUS_AUDIO_FORMAT, MUS_AUDIO_IMIX, MUS_AUDIO_IGAIN, MUS_AUDIO_RECLEV,
      MUS_AUDIO_PCM, MUS_AUDIO_PCM2, MUS_AUDIO_OGAIN, MUS_AUDIO_LINE, MUS_AUDIO_SYNTH,
      MUS_AUDIO_BASS, MUS_AUDIO_TREBLE, MUS_AUDIO_PORT, MUS_AUDIO_SAMPLES_PER_CHANNEL,
      MUS_AUDIO_DIRECTION
};
/* Snd's recorder uses MUS_AUDIO_DIRECTION to find the size of this list */

#if defined(__GNUC__) && (!(defined(__cplusplus)))
  #define MUS_AUDIO_DEVICE_OK(a) ({ int _sndlib_h_2 = a; ((_sndlib_h_2 >= MUS_AUDIO_DEFAULT) && (_sndlib_h_2 <= MUS_AUDIO_DIRECTION)); })
#else
  #define MUS_AUDIO_DEVICE_OK(a) (((a) >= MUS_AUDIO_DEFAULT) && ((a) <= MUS_AUDIO_DIRECTION))
#endif

#define MUS_ERROR -1

enum {MUS_NO_ERROR, MUS_NO_FREQUENCY, MUS_NO_PHASE, MUS_NO_GEN, MUS_NO_LENGTH,
      MUS_NO_FREE, MUS_NO_DESCRIBE, MUS_NO_DATA, MUS_NO_SCALER,
      MUS_MEMORY_ALLOCATION_FAILED, MUS_UNSTABLE_TWO_POLE_ERROR,
      MUS_CANT_OPEN_FILE, MUS_NO_SAMPLE_INPUT, MUS_NO_SAMPLE_OUTPUT,
      MUS_NO_SUCH_CHANNEL, MUS_NO_FILE_NAME_PROVIDED, MUS_NO_LOCATION, MUS_NO_CHANNEL,
      MUS_NO_SUCH_FFT_WINDOW, MUS_UNSUPPORTED_DATA_FORMAT, MUS_HEADER_READ_FAILED,
      MUS_UNSUPPORTED_HEADER_TYPE,
      MUS_FILE_DESCRIPTORS_NOT_INITIALIZED, MUS_NOT_A_SOUND_FILE, MUS_FILE_CLOSED, MUS_WRITE_ERROR,
      MUS_HEADER_WRITE_FAILED, MUS_CANT_OPEN_TEMP_FILE, MUS_INTERRUPTED, MUS_BAD_ENVELOPE,

      MUS_AUDIO_CHANNELS_NOT_AVAILABLE, MUS_AUDIO_SRATE_NOT_AVAILABLE, MUS_AUDIO_FORMAT_NOT_AVAILABLE,
      MUS_AUDIO_NO_INPUT_AVAILABLE, MUS_AUDIO_CONFIGURATION_NOT_AVAILABLE, 
      MUS_AUDIO_NO_LINES_AVAILABLE, MUS_AUDIO_WRITE_ERROR, MUS_AUDIO_SIZE_NOT_AVAILABLE, MUS_AUDIO_DEVICE_NOT_AVAILABLE,
      MUS_AUDIO_CANT_CLOSE, MUS_AUDIO_CANT_OPEN, MUS_AUDIO_READ_ERROR, MUS_AUDIO_AMP_NOT_AVAILABLE,
      MUS_AUDIO_CANT_WRITE, MUS_AUDIO_CANT_READ, MUS_AUDIO_NO_READ_PERMISSION,
      MUS_CANT_CLOSE_FILE, MUS_ARG_OUT_OF_RANGE,
      MUS_MIDI_OPEN_ERROR, MUS_MIDI_READ_ERROR, MUS_MIDI_WRITE_ERROR, MUS_MIDI_CLOSE_ERROR, MUS_MIDI_INIT_ERROR, MUS_MIDI_MISC_ERROR,

      MUS_NO_CHANNELS, MUS_NO_HOP, MUS_NO_WIDTH, MUS_NO_FILE_NAME, MUS_NO_RAMP, MUS_NO_RUN, 
      MUS_NO_INCREMENT, MUS_NO_OFFSET,
      MUS_NO_XCOEFF, MUS_NO_YCOEFF, MUS_NO_XCOEFFS, MUS_NO_YCOEFFS, MUS_NO_RESET,
      MUS_INITIAL_ERROR_TAG};

/* keep this list in sync with mus_error_names in sound.c and snd-test.scm|rb */

#if MUS_DEBUGGING
  #define CALLOC(a, b)  mem_calloc((a), (b), c__FUNCTION__, __FILE__, __LINE__)
  #define MALLOC(a)     mem_malloc((a), c__FUNCTION__, __FILE__, __LINE__)
#ifndef __cplusplus
  #define FREE(a)       a = mem_free(a, c__FUNCTION__, __FILE__, __LINE__)
#else
  #define FREE(a)       mem_free(a, c__FUNCTION__, __FILE__, __LINE__)
#endif
  #define REALLOC(a, b) mem_realloc(a, (b), c__FUNCTION__, __FILE__, __LINE__)
#else
  #define CALLOC(a, b)  calloc((size_t)(a), (size_t)(b))
  #define MALLOC(a)     malloc((size_t)(a))
  #define FREE(a)       free(a)
  #define REALLOC(a, b) realloc(a, (size_t)(b))
#endif

#if MUS_WINDOZE
  #ifdef FOPEN
    #undef FOPEN
  #endif
  #if USE_SND
    #define OPEN(File, Flags, Mode) snd_open((File), (Flags), 0)
  #else
    #define OPEN(File, Flags, Mode) open((File), (Flags))
  #endif
#else
  #if USE_SND
    #define OPEN(File, Flags, Mode) snd_open((File), (Flags), (Mode))
   #else
    #define OPEN(File, Flags, Mode) open((File), (Flags), (Mode))
  #endif
#endif

#if USE_SND
  #define FOPEN(File, Flags)        snd_fopen((File), (Flags))
  #define CREAT(File, Flags)        snd_creat((File), (Flags))
  #define REMOVE(OldF)              snd_remove(OldF, IGNORE_CACHE)
  #define STRERROR(Err)             snd_io_strerror()
  #define CLOSE(Fd, Name)           snd_close(Fd, Name)
  #define FCLOSE(Fd, Name)          snd_fclose(Fd, Name)
#else
  #define FOPEN(File, Flags)        fopen((File), (Flags))
  #define CREAT(File, Flags)        creat((File), (Flags))
  #define REMOVE(OldF)              remove(OldF)
  #define STRERROR(Err)             strerror(Err)
  #define CLOSE(Fd, Name)           close(Fd)
  #define FCLOSE(Fd, Name)          fclose(Fd)
#endif

#ifndef S_setB
  #if HAVE_RUBY
    #define S_setB "set_"
  #endif
  #if HAVE_SCHEME
    #define S_setB "set! "
  #endif
  #if HAVE_FORTH
    #define S_setB "set-"
  #endif
  #if (!HAVE_EXTENSION_LANGUAGE)
    #define S_setB "set-"
  #endif
#endif

#define MUS_LOOP_INFO_SIZE 8

#ifndef Float
  #define Float float
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------- sound.c -------- */

#ifdef __GNUC__
  int mus_error(int error, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
  void mus_print(const char *format, ...)           __attribute__ ((format (printf, 1, 2)));
  char *mus_format(const char *format, ...)         __attribute__ ((format (printf, 1, 2)));
  void mus_snprintf(char *buffer, int buffer_len, const char *format, ...)  __attribute__ ((format (printf, 3, 4)));
#else
  int mus_error(int error, const char *format, ...);
  void mus_print(const char *format, ...);
  char *mus_format(const char *format, ...);
  void mus_snprintf(char *buffer, int buffer_len, const char *format, ...);
#endif

typedef void mus_error_handler_t(int type, char *msg);
mus_error_handler_t *mus_error_set_handler (mus_error_handler_t *new_error_handler);
int mus_make_error(char *error_name);
const char *mus_error_type_to_string(int err);

typedef void mus_print_handler_t(char *msg);
mus_print_handler_t *mus_print_set_handler (mus_print_handler_t *new_print_handler);

off_t mus_sound_samples(const char *arg);
off_t mus_sound_frames(const char *arg);
int mus_sound_datum_size(const char *arg);
off_t mus_sound_data_location(const char *arg);
int mus_sound_chans(const char *arg);
int mus_sound_srate(const char *arg);
int mus_sound_header_type(const char *arg);
int mus_sound_data_format(const char *arg);
int mus_sound_original_format(const char *arg);
off_t mus_sound_comment_start(const char *arg);
off_t mus_sound_comment_end(const char *arg);
off_t mus_sound_length(const char *arg);
int mus_sound_fact_samples(const char *arg);
time_t mus_sound_write_date(const char *arg);
int mus_sound_type_specifier(const char *arg);
int mus_sound_block_align(const char *arg);
int mus_sound_bits_per_sample(const char *arg);

int mus_sound_set_chans(const char *arg, int val);
int mus_sound_set_srate(const char *arg, int val);
int mus_sound_set_header_type(const char *arg, int val);
int mus_sound_set_data_format(const char *arg, int val);
int mus_sound_set_data_location(const char *arg, off_t val);
int mus_sound_set_samples(const char *arg, off_t val);

const char *mus_header_type_name(int type);
const char *mus_data_format_name(int format);
char *mus_header_type_to_string(int type);
char *mus_data_format_to_string(int format);
const char *mus_data_format_short_name(int format);
char *mus_sound_comment(const char *name);
int mus_bytes_per_sample(int format);
float mus_sound_duration(const char *arg);
int mus_sound_initialize(void);
int mus_sample_bits(void);
int mus_sound_override_header(const char *arg, int srate, int chans, int format, int type, off_t location, off_t size);
int mus_sound_forget(const char *name);
int mus_sound_prune(void);
void mus_sound_report_cache(FILE *fp);
int *mus_sound_loop_info(const char *arg);
void mus_sound_set_loop_info(const char *arg, int *loop);

int mus_sound_open_input(const char *arg);
int mus_sound_open_output(const char *arg, int srate, int chans, int data_format, int header_type, const char *comment);
int mus_sound_reopen_output(const char *arg, int chans, int format, int type, off_t data_loc);
int mus_sound_close_input(int fd);
int mus_sound_close_output(int fd, off_t bytes_of_data);
#define mus_sound_seek_frame(Ifd, Frm) mus_file_seek_frame(Ifd, Frm)
#define mus_sound_read(Fd, Beg, End, Chans, Bufs) mus_file_read(Fd, Beg, End, Chans, Bufs)
#define mus_sound_write(Fd, Beg, End, Chans, Bufs) mus_file_write(Fd, Beg, End, Chans, Bufs)

off_t mus_sound_maxamps(const char *ifile, int chans, mus_sample_t *vals, off_t *times);
int mus_sound_set_maxamps(const char *ifile, int chans, mus_sample_t *vals, off_t *times);
bool mus_sound_maxamp_exists(const char *ifile);
int mus_file_to_array(const char *filename, int chan, int start, int samples, mus_sample_t *array);
int mus_array_to_file(const char *filename, mus_sample_t *ddata, int len, int srate, int channels);
char *mus_array_to_file_with_error(const char *filename, mus_sample_t *ddata, int len, int srate, int channels);
int mus_file_to_float_array(const char *filename, int chan, off_t start, int samples, Float *array);
int mus_float_array_to_file(const char *filename, Float *ddata, int len, int srate, int channels);



/* -------- audio.c -------- */

#if (HAVE_OSS || HAVE_ALSA || HAVE_JACK)
  #define ALSA_API 0
  #define OSS_API 1
  #define JACK_API 2
#endif

void mus_audio_describe(void);
char *mus_audio_report(void);
int mus_audio_open_output(int dev, int srate, int chans, int format, int size);
int mus_audio_open_input(int dev, int srate, int chans, int format, int size);
int mus_audio_write(int line, char *buf, int bytes);
int mus_audio_close(int line);
int mus_audio_read(int line, char *buf, int bytes);

int mus_audio_write_buffers(int line, int frames, int chans, mus_sample_t **bufs, int output_format, bool clipped);
int mus_audio_read_buffers(int line, int frames, int chans, mus_sample_t **bufs, int input_format);

int mus_audio_mixer_read(int dev, int field, int chan, float *val);
int mus_audio_mixer_write(int dev, int field, int chan, float *val);
int mus_audio_initialize(void);

#if HAVE_OSS || HAVE_ALSA
  int mus_audio_reinitialize(void); /* 29-Aug-01 for CLM/Snd bugfix? */
  char *mus_alsa_playback_device(void);
  char *mus_alsa_set_playback_device(const char *name);
  char *mus_alsa_capture_device(void);
  char *mus_alsa_set_capture_device(const char *name);
  char *mus_alsa_device(void);
  char *mus_alsa_set_device(const char *name);
  int mus_alsa_buffer_size(void);
  int mus_alsa_set_buffer_size(int size);
  int mus_alsa_buffers(void);
  int mus_alsa_set_buffers(int num);
  bool mus_alsa_squelch_warning(void);
  bool mus_alsa_set_squelch_warning(bool val);
  int mus_audio_api(void);
  void mus_oss_set_buffers(int num, int size);
#endif

int mus_audio_systems(void);
char *mus_audio_system_name(int sys);
char *mus_audio_moniker(void);
int mus_audio_compatible_format(int dev);

#if MUS_SUN
  void mus_sun_set_outputs(int speakers, int headphones, int line_out);
#endif

#if MUS_NETBSD
  void mus_netbsd_set_outputs(int speakers, int headphones, int line_out);
#endif

#if (!HAVE_STRERROR)
  char *strerror(int errnum);
#endif



/* -------- io.c -------- */

int mus_file_open_descriptors(int tfd, const char *arg, int df, int ds, off_t dl, int dc, int dt);
int mus_file_open_read(const char *arg);
bool mus_file_probe(const char *arg);
int mus_file_open_write(const char *arg);
int mus_file_create(const char *arg);
int mus_file_reopen_write(const char *arg);
  int mus_file_close(int fd);
off_t mus_file_seek_frame(int tfd, off_t frame);
int mus_file_read(int fd, int beg, int end, int chans, mus_sample_t **bufs);
int mus_file_read_chans(int fd, int beg, int end, int chans, mus_sample_t **bufs, mus_sample_t **cm);
int mus_file_write(int tfd, int beg, int end, int chans, mus_sample_t **bufs);
int mus_file_read_any(int tfd, int beg, int chans, int nints, mus_sample_t **bufs, mus_sample_t **cm);
int mus_file_read_file(int tfd, int beg, int chans, int nints, mus_sample_t **bufs);
int mus_file_read_buffer(int charbuf_data_format, int beg, int chans, int nints, mus_sample_t **bufs, char *charbuf);
int mus_file_write_file(int tfd, int beg, int end, int chans, mus_sample_t **bufs);
int mus_file_write_buffer(int charbuf_data_format, int beg, int end, int chans, mus_sample_t **bufs, char *charbuf, bool clipped);
char *mus_expand_filename(const char *name);
char *mus_getcwd(void);

bool mus_clipping(void);
bool mus_set_clipping(bool new_value);
bool mus_file_clipping(int tfd);
int mus_file_set_clipping(int tfd, bool clipped);

int mus_file_set_header_type(int tfd, int type);
int mus_file_header_type(int tfd);
char *mus_file_fd_name(int tfd);
int mus_file_set_chans(int tfd, int chans);

Float mus_file_prescaler(int tfd);
Float mus_file_set_prescaler(int tfd, Float val);
Float mus_prescaler(void);
Float mus_set_prescaler(Float new_value);

void mus_bint_to_char(unsigned char *j, int x);
int mus_char_to_bint(const unsigned char *inp);
void mus_lint_to_char(unsigned char *j, int x);
int mus_char_to_lint(const unsigned char *inp);
int mus_char_to_uninterpreted_int(const unsigned char *inp);
void mus_bfloat_to_char(unsigned char *j, float x);
float mus_char_to_bfloat(const unsigned char *inp);
void mus_lfloat_to_char(unsigned char *j, float x);
float mus_char_to_lfloat(const unsigned char *inp);
void mus_bshort_to_char(unsigned char *j, short x);
short mus_char_to_bshort(const unsigned char *inp);
void mus_lshort_to_char(unsigned char *j, short x);
short mus_char_to_lshort(const unsigned char *inp);
void mus_ubshort_to_char(unsigned char *j, unsigned short x);
unsigned short mus_char_to_ubshort(const unsigned char *inp);
void mus_ulshort_to_char(unsigned char *j, unsigned short x);
unsigned short mus_char_to_ulshort(const unsigned char *inp);
double mus_char_to_ldouble(const unsigned char *inp);
double mus_char_to_bdouble(const unsigned char *inp);
void mus_bdouble_to_char(unsigned char *j, double x);
void mus_ldouble_to_char(unsigned char *j, double x);
unsigned int mus_char_to_ubint(const unsigned char *inp);
unsigned int mus_char_to_ulint(const unsigned char *inp);

int mus_iclamp(int lo, int val, int hi);
off_t mus_oclamp(off_t lo, off_t val, off_t hi);
Float mus_fclamp(Float lo, Float val, Float hi);

/* for CLM */
/* these are needed to clear a saved lisp image to the just-initialized state */
void mus_reset_io_c(void);
void mus_reset_headers_c(void);
void mus_reset_audio_c(void);



/* -------- headers.c -------- */

off_t mus_header_samples(void);
off_t mus_header_data_location(void);
int mus_header_chans(void);
int mus_header_srate(void);
int mus_header_type(void);
int mus_header_format(void);
off_t mus_header_comment_start(void);
off_t mus_header_comment_end(void);
int mus_header_type_specifier(void);
int mus_header_bits_per_sample(void);
int mus_header_fact_samples(void);
int mus_header_block_align(void);
int mus_header_loop_mode(int which);
int mus_header_loop_start(int which);
int mus_header_loop_end(int which);
int mus_header_mark_position(int id);
int mus_header_base_note(void);
int mus_header_base_detune(void);
void mus_header_set_raw_defaults(int sr, int chn, int frm);
void mus_header_raw_defaults(int *sr, int *chn, int *frm);
off_t mus_header_true_length(void);
int mus_header_original_format(void);
off_t mus_samples_to_bytes(int format, off_t size);
off_t mus_bytes_to_samples(int format, off_t size);
int mus_header_write_next_header(int chan, int srate, int chans, int loc, int siz, int format, const char *comment, int len);
int mus_header_read(const char *name);
int mus_header_write(const char *name, int type, int srate, int chans, off_t loc, off_t size_in_samples, int format, const char *comment, int len);
off_t mus_header_aux_comment_start(int n);
off_t mus_header_aux_comment_end(int n);
int mus_header_initialize(void);
bool mus_header_writable(int type, int format);
void mus_header_set_aiff_loop_info(int *data);
int mus_header_sf2_entries(void);
char *mus_header_sf2_name(int n);
int mus_header_sf2_start(int n);
int mus_header_sf2_end(int n);
int mus_header_sf2_loop_start(int n);
int mus_header_sf2_loop_end(int n);
const char *mus_header_original_format_name(int format, int type);
bool mus_header_no_header(const char *name);

char *mus_header_riff_aux_comment(const char *name, off_t *starts, off_t *ends);
char *mus_header_aiff_aux_comment(const char *name, off_t *starts, off_t *ends);

int mus_header_change_chans(const char *filename, int type, int new_chans);
int mus_header_change_srate(const char *filename, int type, int new_srate);
int mus_header_change_type(const char *filename, int new_type, int new_format);
int mus_header_change_format(const char *filename, int type, int new_format);
int mus_header_change_location(const char *filename, int type, off_t new_location);
int mus_header_change_comment(const char *filename, int type, char *new_comment);
int mus_header_change_data_size(const char *filename, int type, off_t bytes);

typedef void mus_header_write_hook_t(const char *filename);
mus_header_write_hook_t *mus_header_write_set_hook(mus_header_write_hook_t *new_hook);


/* -------- midi.c -------- */
int mus_midi_open_read(const char *name);
int mus_midi_open_write(const char *name);
int mus_midi_close(int line);
int mus_midi_read(int line, unsigned char *buffer, int bytes);
int mus_midi_write(int line, unsigned char *buffer, int bytes);
char *mus_midi_device_name(int sysdev);
char *mus_midi_describe(void);
#if HAVE_EXTENSION_LANGUAGE
  void mus_midi_init(void);
#endif


#if MUS_DEBUGGING
  /* snd-utils.c (only used in conjunction with Snd's memory tracking functions) */
  void *mem_calloc(int len, int size, const char *func, const char *file, int line);
  void *mem_malloc(int len, const char *func, const char *file, int line);
  void *mem_free(void *ptr, const char *func, const char *file, int line);
  void *mem_realloc(void *ptr, int size, const char *func, const char *file, int line);
#endif

#if (!HAVE_STRDUP)
char *strdup(const char *str);
#endif
#if (!HAVE_FILENO)
int fileno(FILE *fp);
#endif

#ifdef __cplusplus
}
#endif

#endif
