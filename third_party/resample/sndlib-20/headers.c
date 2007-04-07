/* readers/writers for various sound file headers
 *
 * --------------------------------
 * int mus_header_read (const char *name)
 * int mus_header_write (const char *name, int type, int in_srate, int in_chans, off_t loc, off_t size_in_samples, int format, const char *comment, int len)
 * int mus_header_initialize (void)
 *
 *   Once mus_header_read has been called, the data in it can be accessed through:
 *
 *        off_t mus_header_samples (void)                       samples 
 *        off_t mus_header_data_location (void)                 location of data (bytes)
 *        int mus_header_chans (void)                           channels
 *        int mus_header_srate (void)                           srate
 *        int mus_header_type (void)                            header type (i.e. aiff, wave, etc)  (see sndlib.h)
 *        int mus_header_format (void)                          data format (see sndlib.h)
 *        off_t mus_header_comment_start (void)                 comment start location (if any) (bytes)
 *        off_t mus_header_comment_end (void)                   comment end location
 *        off_t mus_header_aux_comment_start (int n)            if multiple comments, nth start location
 *        off_t mus_header_aux_comment_end (int n)              if multiple comments, nth end location
 *        int mus_header_type_specifier (void)                  original (header-specific) type ID
 *        int mus_header_bits_per_sample (void)                 sample width in bits
 *        off_t mus_header_true_length (void)                   true (lseek) file length
 *        int mus_bytes_per_sample (int format)                 sample width in bytes
 *        bool mus_header_writable(int type, int format)        can we write this header
 * --------------------------------
 *
 *   "Linear" below means 2's complement integer.
 *
 * Currently supported read/write (in standard data formats):                         
 *      NeXT/Sun/DEC/AFsp
 *      AIFF/AIFC
 *      RIFF (microsoft wave)
 *      IRCAM (old style)
 *      NIST-sphere
 *      no header
 *
 * Currently supported read-only (in selected data formats):
 *      8SVX (IFF), EBICSF, INRS, ESPS, SPPACK, ADC (OGI), AVR, VOC, CSL, snack "SMP", PVF,
 *      Sound Tools, Turtle Beach SMP, SoundFont 2.0, Sound Designer I, PSION alaw, MAUD, 
 *      Gravis Ultrasound, Comdisco SPW, Goldwave sample, OMF, NVF,
 *      Sonic Foundry, SBStudio II, Delusion digital, Digiplayer ST3, Farandole Composer WaveSample,
 *      Ultratracker WaveSample, Sample Dump exchange, Yamaha SY85 and SY99 (buggy), Yamaha TX16W, 
 *      Covox v8, AVI, Kurzweil 2000, Paris Ensoniq, Impulse tracker, Korg, Akai type 4, Maui,
 *
 * for a few of these I'm still trying to get documentation -- best sources of info
 * are ftp.cwi.nl:pub/audio (info files), the AFsp sources, and the SOX sources.
 * sox and gsm are at ftp.cwi.nl, AFsp is from kabal@Polaris.EE.McGill.CA (Peter Kabal) as
 * ftp.TSP.EE.McGill.CA:/pub/AFsp/AFsp-V3R2.tar.Z.  The Sound Designer formats are described
 * in the "Developer Documentation" from Digidesign.  Other useful sources can be found at
 * ftp.x.org:/contrib/audio/nas, svr-ftp.eng.cam.ac.uk:/comp.speech/tools, and
 * at http://www.wotsit.org.  I put many of my test cases in 
 * ccrma-ftp.stanford.edu:/pub/Lisp/sf.tar.gz.  The RIFF format is described in the 
 * Microsoft Multimedia Programmer's Reference Manual at ftp.microsoft.com:/SoftLib/MSLFILES/MDRK.EXE.
 * AVI format is described in http://www.rahul.net/jfm/avi.html.
 *
 * For a lot of info and examples see http://www.TSP.ECE.McGill.CA/MMSP/Documents/AudioFormats/index.html
 *
 * The main problem with compressed sound files is that you can't do reliable
 * random access to the data, can't easily read backwards, and most of the compression
 * schemes are proprietary (and appalling), but to translate Mus10/Sam, HCOM, IEEE text, 
 * MIDI sample dumps, various adpcm cases, NIST shortpack files, and AVI see snd-trans.c 
 * in the sound editor (snd-8.tar.gz).
 *
 * If anyone has information on any other header or data formats, I would be most interested in it,
 * but only if it can be included in this file.
 *
 * ivc format appears to have 16 bytes of header (-1 5 0 0 0 0 -> mulaw) followed by mulaw or alaw data
 */

#include <mus-config.h>

#if USE_SND
  #include "snd.h"
#else
  #if HAVE_RUBY && (!CLM)
    #include "xen.h"
  #endif
#endif

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#if HAVE_STRING_H
  #include <string.h>
#endif
#if (defined(HAVE_LIBC_H) && (!defined(HAVE_UNISTD_H)))
  #include <libc.h>
#else
  #if (!(defined(_MSC_VER)))
    #include <unistd.h>
  #endif
#endif

#include "_sndlib.h"
#include "sndlib-strings.h"

static bool hdrbuf_is_inited = false;

#define HDRBUFSIZ 256
static unsigned char *hdrbuf;
#define INITIAL_READ_SIZE 32

/* AIFF files can have any number of ANNO chunks, so we'll grab at least 4 of them */
#define AUX_COMMENTS 4
static off_t *aux_comment_start = NULL, *aux_comment_end = NULL;

#define LOOPS 2
static int *loop_modes = NULL, *loop_starts = NULL, *loop_ends = NULL;
static int markers = 0;
static int *marker_ids = NULL, *marker_positions = NULL;

/* for CLM */
void mus_reset_headers_c(void) 
{
  hdrbuf_is_inited = false; 
  markers = 0;
}

int mus_header_initialize(void)
{
  if (!hdrbuf_is_inited)
    {
      hdrbuf_is_inited = true;
      hdrbuf = (unsigned char *)CALLOC(HDRBUFSIZ, sizeof(unsigned char));
      aux_comment_start = (off_t *)CALLOC(AUX_COMMENTS, sizeof(off_t));
      aux_comment_end = (off_t *)CALLOC(AUX_COMMENTS, sizeof(off_t));
      loop_modes = (int *)CALLOC(LOOPS, sizeof(int));
      loop_starts = (int *)CALLOC(LOOPS, sizeof(int));
      loop_ends = (int *)CALLOC(LOOPS, sizeof(int));
      if ((hdrbuf == NULL) || (aux_comment_start == NULL) || (aux_comment_end == NULL) ||
	  (loop_modes == NULL) || (loop_starts == NULL) || (loop_ends == NULL))
	return(mus_error(MUS_MEMORY_ALLOCATION_FAILED, "mus_header_initialize: buffer allocation failed"));
    }
  return(MUS_NO_ERROR);
}


static const unsigned char I_DSND[4] = {'.','s','n','d'};  /* NeXT/Sun/Dec/SGI/AFsp first word */
static const unsigned char I_FORM[4] = {'F','O','R','M'};  /* AIFF first word */
static const unsigned char I_AIFF[4] = {'A','I','F','F'};  /* AIFF second word */
static const unsigned char I_AIFC[4] = {'A','I','F','C'};  /* ditto but might be compressed data */
static const unsigned char I_COMM[4] = {'C','O','M','M'};
static const unsigned char I_COMT[4] = {'C','O','M','T'};
static const unsigned char I_INFO[4] = {'I','N','F','O'};
static const unsigned char I_INST[4] = {'I','N','S','T'};
static const unsigned char I_inst[4] = {'i','n','s','t'};  /* RIFF wants lower case, just to be different */
static const unsigned char I_MARK[4] = {'M','A','R','K'};
static const unsigned char I_SSND[4] = {'S','S','N','D'};
static const unsigned char I_FVER[4] = {'F','V','E','R'};
static const unsigned char I_NONE[4] = {'N','O','N','E'};
static const unsigned char I_ULAW[4] = {'U','L','A','W'};  /* AIFC compression types that we can handle */
static const unsigned char I_ulaw[4] = {'u','l','a','w'};  /* or maybe it's lowercase (Apple) ... */
static const unsigned char I_ima4[4] = {'i','m','a','4'};  /* AIFC IMA adpcm apparently */
static const unsigned char I_raw_[4] = {'r','a','w',' '};  /* AIFC offset binary OS 8.5 (others are 'MAC3' 'MAC6' 'cdx4' 'cdx2' 'str4') */
static const unsigned char I_sowt[4] = {'s','o','w','t'};  /* AIFC 16-bit little endian -- used by Mac when extracting CD tracks */
static const unsigned char I_in32[4] = {'i','n','3','2'};  /* AIFC */
static const unsigned char I_in24[4] = {'i','n','2','4'};  /* AIFC */
static const unsigned char I_ni23[4] = {'n','i','2','3'};  /* AIFC */
static const unsigned char I_fl32[4] = {'f','l','3','2'};  /* AIFC 32-bit float */
static const unsigned char I_FL32[4] = {'F','L','3','2'};  /* AIFC 32-bit float (apparently used by CSound and SoundHack) */
static const unsigned char I_fl64[4] = {'f','l','6','4'};  /* AIFC 64-bit float */
static const unsigned char I_twos[4] = {'t','w','o','s'};  /* AIFC big endian? */
static const unsigned char I_ALAW[4] = {'A','L','A','W'};
static const unsigned char I_alaw[4] = {'a','l','a','w'};  /* apple */
static const unsigned char I_APPL[4] = {'A','P','P','L'};
static const unsigned char I_MUS_[4] = {'C','L','M',' '};  /* I hereby claim this AIFF chunk name */
static const unsigned char I_RIFF[4] = {'R','I','F','F'};  /* RIFF first word */
static const unsigned char I_RIFX[4] = {'R','I','F','X'};  /* RIFX first word (big-endian RIFF file) */
static const unsigned char I_WAVE[4] = {'W','A','V','E'};
static const unsigned char I_fmt_[4] = {'f','m','t',' '};
static const unsigned char I_data[4] = {'d','a','t','a'};
static const unsigned char I_fact[4] = {'f','a','c','t'};  /* used by compressed RIFF files */
static const unsigned char I_clm_[4] = {'c','l','m',' '};
static const unsigned char I_NIST[4] = {'N','I','S','T'};  /* first word of NIST SPHERE files */
static const unsigned char I_8SVX[4] = {'8','S','V','X'};  /* AIFF other choice */
static const unsigned char I_16SV[4] = {'1','6','S','V'};  /* hmmm... 16-bit 8svx? */
static const unsigned char I_VOC0[4] = {'C','r','e','a'};  /* Actual text is "Creative Voice File" */
static const unsigned char I_VOC1[4] = {'t','i','v','e'};
static const unsigned char I_SOUN[4] = {'S','O','U','N'};  /* Sound Tools first word="SOUND" -- not unique as SMP files start with "SOUND SAMPLE" */
static const unsigned char I_D_SA[4] = {'D',' ','S','A'};
static const unsigned char I_MPLE[4] = {'M','P','L','E'};
static const unsigned char I_BODY[4] = {'B','O','D','Y'};  /* next 4 for 8svx chunk names */
static const unsigned char I_VHDR[4] = {'V','H','D','R'};
static const unsigned char I_CHAN[4] = {'C','H','A','N'};
static const unsigned char I_ANNO[4] = {'A','N','N','O'};
static const unsigned char I_NAME[4] = {'N','A','M','E'};
static const unsigned char I_AVR_[4] = {'2','B','I','T'};  /* first word of AVR files */
static const unsigned char I_HCOM[4] = {'H','C','O','M'};
static const unsigned char I_FSSD[4] = {'F','S','S','D'};
static const unsigned char I_SPIB[4] = {'%','/','/','\n'}; /* first word of IEEE spib text sound files */
static const unsigned char I_S___[4] = {'%','-','-','-'};  /* first word of other IEEE spib text sound files */
static const unsigned char I_ALaw[4] = {'A','L','a','w'};  /* first word of PSION alaw files */
static const unsigned char I_Soun[4] = {'S','o','u','n'};  /* second */
static const unsigned char I_MAUD[4] = {'M','A','U','D'};  /* MAUD specialization of AIFF */
static const unsigned char I_MHDR[4] = {'M','H','D','R'};
static const unsigned char I_MDAT[4] = {'M','D','A','T'};
static const unsigned char I_mdat[4] = {'m','d','a','t'};  /* quicktime */
static const unsigned char I_MThd[4] = {'M','T','h','d'};  /* sigh -- the M word */
static const unsigned char I_DECN[4] = {'.','s','d','\0'}; /* first word of DEC files (?) */
static const unsigned char I_sfbk[4] = {'s','f','b','k'};  /* SoundFont 2.0 */
static const unsigned char I_sdta[4] = {'s','d','t','a'};
static const unsigned char I_shdr[4] = {'s','h','d','r'};
static const unsigned char I_pdta[4] = {'p','d','t','a'};
static const unsigned char I_LIST[4] = {'L','I','S','T'};
static const unsigned char I_GF1P[4] = {'G','F','1','P'};  /* first word of Gravis Ultrsound patch files */
static const unsigned char I_ATCH[4] = {'A','T','C','H'};  /* second word */
static const unsigned char I_DSIG[4] = {'$','S','I','G'};  /* first word of Comdisco SPW file */
static const unsigned char I_NAL_[4] = {'N','A','L','_'};  /* second word */
static const unsigned char I_GOLD[4] = {'G','O','L','D'};  /* first word Goldwave(?) sample file */
static const unsigned char I__WAV[4] = {' ','S','A','M'};  /* second word */
static const unsigned char I_SRFS[4] = {'S','R','F','S'};  /* first word Sonic Resource Foundry file(?) */
static const unsigned char I_Diam[4] = {'D','i','a','m'};  /* first word DiamondWare file */
static const unsigned char I_ondW[4] = {'o','n','d','W'};  /* second word */
static const unsigned char I_CSRE[4] = {'C','S','R','E'};  /* adf first word -- second starts with "40" */
static const unsigned char I_SND_[4] = {'S','N','D',' '};  /* SBStudio II */
static const unsigned char I_SNIN[4] = {'S','N','I','N'};
static const unsigned char I_SNNA[4] = {'S','N','N','A'};
static const unsigned char I_SNDT[4] = {'S','N','D','T'};
static const unsigned char I_DDSF[4] = {'D','D','S','F'};  /* Delusion Digital Sound File */
static const unsigned char I_FSMt[4] = {'F','S','M',(unsigned char)'\376'};  /* Farandole Composer WaveSample */
static const unsigned char I_SDXc[4] = {'S','D','X',':'};  /* Sample dump exchange format */
static const unsigned char I_UWFD[4] = {'U','W','F','D'};  /* Ultratracker Wavesample */
static const unsigned char I_LM89[4] = {'L','M','8','9'};  /* Yamaha TX-16 */
static const unsigned char I_SY80[4] = {'S','Y','8','0'};  /* Yamaha SY-99 */
static const unsigned char I_SY85[4] = {'S','Y','8','5'};  /* Yamaha SY-85 */
static const unsigned char I_SCRS[4] = {'S','C','R','S'};  /* Digiplayer ST3 */
static const unsigned char I_covox[4] = {(unsigned char)'\377','\125',(unsigned char)'\377',(unsigned char)'\252'};
/* static const unsigned char I_DSPL[4] = {'D','S','P','L'};  */ /* Digitracker SPL (now obsolete) */
static const unsigned char I_AVI_[4] = {'A','V','I',' '};  /* RIFF AVI */
static const unsigned char I_strf[4] = {'s','t','r','f'};  
static const unsigned char I_movi[4] = {'m','o','v','i'};  
static const unsigned char I_PRAM[4] = {'P','R','A','M'};  /* Kurzweil 2000 */
static const unsigned char I_ones[4] = {(unsigned char)'\377',(unsigned char)'\377',(unsigned char)'\377',(unsigned char)'\377'};
static const unsigned char I_zeros[4] = {'\0','\0','\0','\0'};
static const unsigned char I_asf0[4] = {(unsigned char)'\321','\051',(unsigned char)'\342',(unsigned char)'\326'};
static const unsigned char I_asf1[4] = {(unsigned char)'\332','\065',(unsigned char)'\321','\021'};
static const unsigned char I_asf2[4] = {(unsigned char)'\220','\064','\000',(unsigned char)'\240'};
static const unsigned char I_asf3[4] = {(unsigned char)'\311','\003','\111',(unsigned char)'\276'};
static const unsigned char I__PAF[4] = {' ','p','a','f'};  /* Paris Ensoniq */
static const unsigned char I_FAP_[4] = {'f','a','p',' '};  /* Paris Ensoniq */
static const unsigned char I_DS16[4] = {'D','S','1','6'};  /* CSL */
static const unsigned char I_HEDR[4] = {'H','E','D','R'};  
static const unsigned char I_HDR8[4] = {'H','D','R','8'};  
static const unsigned char I_SDA_[4] = {'S','D','A','_'};  
static const unsigned char I_SDAB[4] = {'S','D','A','B'};  
static const unsigned char I_SD_B[4] = {'S','D','_','B'};  
static const unsigned char I_NOTE[4] = {'N','O','T','E'};  
static const unsigned char I_file[4] = {'f','i','l','e'};  /* snack "SMP" */
static const unsigned char I__sam[4] = {'=','s','a','m'};  
static const unsigned char I_SU7M[4] = {'S','U','7','M'};  
static const unsigned char I_SU7R[4] = {'S','U','7','R'};  
static const unsigned char I_PVF1[4] = {'P','V','F','1'};  /* portable voice format (mgetty) */
static const unsigned char I_PVF2[4] = {'P','V','F','2'};
static const unsigned char I_AUTH[4] = {'A','U','T','H'};
static const unsigned char I_riff[4] = {'r','i','f','f'};  /* SoundForge */
static const unsigned char I_TWIN[4] = {'T','W','I','N'};  /* TwinVQ */
static const unsigned char I_IMPS[4] = {'I','M','P','S'};  /* Impulse Tracker */
static const unsigned char I_SMP1[4] = {'S','M','P','1'};  /* Korg */
static const unsigned char I_Maui[4] = {'M','a','u','i'};  /* Turtle Beach */
static const unsigned char I_SDIF[4] = {'S','D','I','F'};  /* IRCAM sdif */
static const unsigned char I_NVF_[4] = {'N','V','F',' '};  /* Nomad II Creative NVF */
static const unsigned char I_VFMT[4] = {'V','F','M','T'};  /* Nomad II Creative NVF */
static const unsigned char I_OggS[4] = {'O','g','g','S'};  /* Ogg-related files, apparently -- ogg123 has "vorbis" instead of "Speex" */
static const unsigned char I_fLaC[4] = {'f','L','a','C'};  /* FLAC */
static const unsigned char I_ajkg[4] = {'a','j','k','g'};  /* shorten */
static const unsigned char I_TTA1[4] = {'T','T','A','1'};  /* ttaenc */
static const unsigned char I_wvpk[4] = {'w','v','p','k'};  /* wavpack */

#define I_IRCAM_VAX  0x0001a364
#define I_IRCAM_SUN  0x0002a364
#define I_IRCAM_MIPS 0x0003a364
#define I_IRCAM_NEXT  0x0004a364

#define NINRS	7
static const unsigned int I_INRS[NINRS] = {0xcb460020, 0xd0465555, 0xfa460000, 0x1c470040, 0x3b470080, 0x7a470000, 0x9c470040};

static off_t data_location = 0;
static int srate = 0, chans = 0, header_type = MUS_UNSUPPORTED, data_format = MUS_UNKNOWN, original_data_format = 0;
static int type_specifier = 0, bits_per_sample = 0, block_align = 0, fact_samples = 0;
static off_t comment_start = 0, comment_end = 0;
static off_t true_file_length = 0, data_size = 0;
static int base_detune = 0, base_note = 0;
static bool little_endian = false;

off_t mus_header_samples(void) {return(data_size);}
off_t mus_header_data_location(void) {return(data_location);}
int mus_header_chans(void) {return(chans);}
int mus_header_srate(void) {return(srate);}
int mus_header_type(void) {return(header_type);}
int mus_header_format(void) {return(data_format);}
off_t mus_header_comment_start(void) {return(comment_start);}
off_t mus_header_comment_end(void) {return(comment_end);}
off_t mus_header_aux_comment_start(int n) {if (aux_comment_start) return(aux_comment_start[n]); else return(-1);}
off_t mus_header_aux_comment_end(int n) {if (aux_comment_end) return(aux_comment_end[n]); else return(-1);}
int mus_header_type_specifier(void) {return(type_specifier);}
int mus_header_bits_per_sample(void) {return(bits_per_sample);}
int mus_header_fact_samples(void) {return(fact_samples);}
int mus_header_block_align(void) {return(block_align);}
off_t mus_header_true_length(void) {return(true_file_length);}
int mus_header_original_format(void) {return(original_data_format);}
int mus_header_loop_mode(int which) {if (loop_modes) return(loop_modes[which]); else return(-1);}
int mus_header_loop_start(int which) {if (loop_starts) return(loop_starts[which]); else return(-1);}
int mus_header_loop_end(int which) {if (loop_ends) return(loop_ends[which]); else return(-1);}
int mus_header_mark_position(int id) {int i; for (i = 0; i < markers; i++) {if (marker_ids[i] == id) return(marker_positions[i]);} return(-1);}
int mus_header_base_detune(void) {return(base_detune);}
int mus_header_base_note(void) {return(base_note);}

int mus_bytes_per_sample(int format)
{
  switch (format)
    {
    case MUS_BYTE:             return(1); break;
    case MUS_BSHORT:           return(2); break;
    case MUS_UBYTE:            return(1); break;
    case MUS_MULAW:            return(1); break;
    case MUS_ALAW:             return(1); break;
    case MUS_BINT:             return(4); break;
    case MUS_BFLOAT:           return(4); break;
    case MUS_BFLOAT_UNSCALED:  return(4); break;
    case MUS_B24INT:           return(3); break;
    case MUS_BDOUBLE:          return(8); break;
    case MUS_BDOUBLE_UNSCALED: return(8); break;
    case MUS_LSHORT:           return(2); break;
    case MUS_LINT:             return(4); break;
    case MUS_LFLOAT:           return(4); break;
    case MUS_LDOUBLE:          return(8); break;
    case MUS_LFLOAT_UNSCALED:  return(4); break;
    case MUS_LDOUBLE_UNSCALED: return(8); break;
    case MUS_L24INT:           return(3); break;
    case MUS_UBSHORT:          return(2); break;
    case MUS_ULSHORT:          return(2); break;
    case MUS_BINTN:            return(4); break;
    case MUS_LINTN:            return(4); break;
    default:                   return(1); break; /* we divide by this number, so 0 is not safe */
    }
}

off_t mus_samples_to_bytes (int format, off_t size) {return(size * (mus_bytes_per_sample(format)));}
off_t mus_bytes_to_samples (int format, off_t size) {return((off_t)(size / (mus_bytes_per_sample(format))));}


static bool equal_big_or_little_endian(const unsigned char *n1, const unsigned int n2)
{
  return((mus_char_to_ubint(n1) == n2) || (mus_char_to_ulint(n1) == n2));
}

static short big_or_little_endian_short(const unsigned char *n, bool little)
{
  if (little) return(mus_char_to_lshort(n));
  return(mus_char_to_bshort(n));
}

static int big_or_little_endian_int(const unsigned char *n, bool little)
{
  if (little) return(mus_char_to_lint(n));
  return(mus_char_to_bint(n));
}

static unsigned int big_or_little_endian_uint(const unsigned char *n, bool little)
{
  if (little) return(mus_char_to_ulint(n));
  return(mus_char_to_ubint(n));
}

static float big_or_little_endian_float(const unsigned char *n, bool little)
{
  if (little) return(mus_char_to_lfloat(n));
  return(mus_char_to_bfloat(n));
}

static bool match_four_chars(const unsigned char *head, const unsigned char *match)
{ 
  return((head[0] == match[0]) &&
	 (head[1] == match[1]) &&
	 (head[2] == match[2]) &&
	 (head[3] == match[3]));
}
  
static void write_four_chars(unsigned char *head, const unsigned char *match)
{
  head[0] = match[0];
  head[1] = match[1];
  head[2] = match[2];
  head[3] = match[3];
}

const char *mus_header_type_name(int type)
{
  switch (type)
    {
    case MUS_NEXT:             return("Sun/Next");                break;
    case MUS_AIFC:             return("AIFC");                    break;
    case MUS_RIFF:             return("RIFF");                    break;
    case MUS_BICSF:            return("BICSF");                   break;
    case MUS_NIST:             return("NIST");                    break;
    case MUS_INRS:             return("INRS");                    break;
    case MUS_ESPS:             return("ESPS");                    break;
    case MUS_SVX:              return("SVX8");                    break;
    case MUS_VOC:              return("VOC");                     break;
    case MUS_SNDT:             return("SNDT");                    break;
    case MUS_RAW:              return("raw (no header)");         break;
    case MUS_SMP:              return("SMP");                     break;
    case MUS_AVR:              return("AVR");                     break;
    case MUS_IRCAM:            return("IRCAM");                   break;
    case MUS_SD1:              return("Sound Designer 1");        break;
    case MUS_SPPACK:           return("SPPACK");                  break;
    case MUS_MUS10:            return("Mus10");                   break;
    case MUS_HCOM:             return("HCOM");                    break;
    case MUS_PSION:            return("PSION");                   break;
    case MUS_MAUD:             return("MAUD");                    break;
    case MUS_IEEE:             return("IEEE text");               break;
    case MUS_MATLAB:           return("Matlab");                  break;
    case MUS_ADC:              return("ADC/OGI");                 break;
    case MUS_MIDI:             return("MIDI");                    break;
    case MUS_SOUNDFONT:        return("SoundFont");               break;
    case MUS_GRAVIS:           return("Gravis Ultrasound patch"); break;
    case MUS_COMDISCO:         return("Comdisco SPW signal");     break;
    case MUS_GOLDWAVE:         return("Goldwave sample");         break;
    case MUS_SRFS:             return("SRFS");                    break;
    case MUS_MIDI_SAMPLE_DUMP: return("MIDI sample dump");        break;
    case MUS_DIAMONDWARE:      return("DiamondWare");             break;
    case MUS_ADF:              return("CSRE adf");                break;
    case MUS_SBSTUDIOII:       return("SBStudioII");              break;
    case MUS_DELUSION:         return("Delusion");                break;
    case MUS_FARANDOLE:        return("Farandole");               break;
    case MUS_SAMPLE_DUMP:      return("Sample dump");             break;
    case MUS_ULTRATRACKER:     return("Ultratracker");            break;
    case MUS_YAMAHA_TX16W:     return("TX-16W");                  break;
    case MUS_YAMAHA_SY85:      return("Sy-85");                   break;
    case MUS_YAMAHA_SY99:      return("Sy-99");                   break;
    case MUS_KURZWEIL_2000:    return("Kurzweil 2000");           break;
    case MUS_KORG:             return("Korg");                    break;
    case MUS_MAUI:             return("Turtle Beach");            break;
    case MUS_IMPULSETRACKER:   return("Impulse Tracker");         break;
    case MUS_AKAI4:            return("AKAI 4");                  break;
    case MUS_DIGIPLAYER:       return("Digiplayer ST3");          break;
    case MUS_COVOX:            return("Covox V8");                break;
    case MUS_AVI:              return("AVI");                     break;
    case MUS_OMF:              return("OMF");                     break;
    case MUS_QUICKTIME:        return("Quicktime");               break;
    case MUS_ASF:              return("asf");                     break;
    case MUS_AIFF:             return("AIFF");                    break;
    case MUS_PAF:              return("Ensoniq Paris");           break;
    case MUS_CSL:              return("CSL");                     break;
    case MUS_FILE_SAMP:        return("snack SMP");               break;
    case MUS_PVF:              return("Portable Voice Format");   break;
    case MUS_SOUNDFORGE:       return("SoundForge");              break;
    case MUS_TWINVQ:           return("TwinVQ");                  break;
    case MUS_SDIF:             return("IRCAM sdif");              break;
    case MUS_NVF:              return("Creative NVF");            break;
    case MUS_OGG:              return("Ogg Vorbis");              break;
    case MUS_FLAC:             return("Flac");                    break;
    case MUS_SPEEX:            return("Speex");                   break;
    case MUS_MPEG:             return("mpeg");                    break;
    case MUS_SHORTEN:          return("shorten");                 break;
    case MUS_TTA:              return("tta");                     break;
    case MUS_WAVPACK:          return("wavpack");                 break;
    default:                   return("unsupported");             break;
    }
}

const char *mus_data_format_name(int format)
{
  switch (format)
    {
    case MUS_BSHORT:           return("big endian short (16 bits)");               break;
    case MUS_MULAW:            return("mulaw (8 bits)");                           break;
    case MUS_BYTE:             return("signed byte (8 bits)");                     break;
    case MUS_BFLOAT:           return("big endian float (32 bits)");               break;
    case MUS_BFLOAT_UNSCALED:  return("big endian float (32 bits, unscaled)");     break;
    case MUS_BINT:             return("big endian int (32 bits)");                 break;
    case MUS_ALAW:             return("alaw (8 bits)");                            break;
    case MUS_UBYTE:            return("unsigned byte (8 bits)");                   break;
    case MUS_B24INT:           return("big endian int (24 bits)");                 break;
    case MUS_BDOUBLE:          return("big endian double (64 bits)");              break;
    case MUS_BDOUBLE_UNSCALED: return("big endian double (64 bits, unscaled)");    break;
    case MUS_LSHORT:           return("little endian short (16 bits)");            break;
    case MUS_LINT:             return("little endian int (32 bits)");              break;
    case MUS_LFLOAT:           return("little endian float (32 bits)");            break;
    case MUS_LDOUBLE:          return("little endian double (64 bits)");           break;
    case MUS_LFLOAT_UNSCALED:  return("little endian float (32 bits, unscaled)");  break;
    case MUS_LDOUBLE_UNSCALED: return("little endian double (64 bits, unscaled)"); break;
    case MUS_UBSHORT:          return("unsigned big endian short (16 bits)");      break;
    case MUS_ULSHORT:          return("unsigned little endian short (16 bits)");   break;
    case MUS_L24INT:           return("little endian int (24 bits)");              break;
    case MUS_BINTN:            return("normalized big endian int (32 bits)");      break;
    case MUS_LINTN:            return("normalized little endian int (32 bits)");   break;
    default:                   return("unknown");                                  break;
    }
}

const char *mus_data_format_short_name(int format)
{
  switch (format)
    {
    case MUS_BSHORT:           return("short int");       break;
    case MUS_MULAW:            return("mulaw");           break;
    case MUS_BYTE:             return("signed byte");     break;
    case MUS_BFLOAT:           return("float");           break;
    case MUS_BFLOAT_UNSCALED:  return("float unscaled)"); break;
    case MUS_BINT:             return("int");             break;
    case MUS_ALAW:             return("alaw");            break;
    case MUS_UBYTE:            return("unsigned byte");   break;
    case MUS_B24INT:           return("24-bit int");      break;
    case MUS_BDOUBLE:          return("double");          break;
    case MUS_BDOUBLE_UNSCALED: return("double unscaled"); break;
    case MUS_LSHORT:           return("short int");       break;
    case MUS_LINT:             return("int");             break;
    case MUS_LFLOAT:           return("float");           break;
    case MUS_LDOUBLE:          return("double");          break;
    case MUS_LFLOAT_UNSCALED:  return("float unscaled");  break;
    case MUS_LDOUBLE_UNSCALED: return("double unscaled"); break;
    case MUS_UBSHORT:          return("unsigned short");  break;
    case MUS_ULSHORT:          return("unsigned short");  break;
    case MUS_L24INT:           return("24-bit int");      break;
    case MUS_BINTN:            return("normalized int");  break;
    case MUS_LINTN:            return("normalized int");  break;
    default:                   return("unknown");         break;
    }
}

#if HAVE_RUBY
  #define TO_LANG(Str) strdup(xen_scheme_constant_to_ruby(Str))
#else
  #define TO_LANG(Str) Str
#endif

char *mus_header_type_to_string(int type)
{
  switch (type)
    {
    case MUS_NEXT:      return(TO_LANG(S_mus_next));
    case MUS_AIFF:      return(TO_LANG(S_mus_aiff));
    case MUS_AIFC:      return(TO_LANG(S_mus_aifc));
    case MUS_RIFF:      return(TO_LANG(S_mus_riff));
    case MUS_NIST:      return(TO_LANG(S_mus_nist));
    case MUS_IRCAM:     return(TO_LANG(S_mus_ircam));
    case MUS_RAW:       return(TO_LANG(S_mus_raw));
    case MUS_BICSF:     return(TO_LANG(S_mus_bicsf));
    case MUS_VOC:       return(TO_LANG(S_mus_voc));
    case MUS_SVX:       return(TO_LANG(S_mus_svx));
    case MUS_SOUNDFONT: return(TO_LANG(S_mus_soundfont));
    }
  return(NULL);
}

char *mus_data_format_to_string(int format)
{
  switch (format)
    {
    case MUS_BSHORT:           return(TO_LANG(S_mus_bshort));
    case MUS_LSHORT:           return(TO_LANG(S_mus_lshort));
    case MUS_MULAW:            return(TO_LANG(S_mus_mulaw));
    case MUS_ALAW:             return(TO_LANG(S_mus_alaw));
    case MUS_BYTE:             return(TO_LANG(S_mus_byte));
    case MUS_UBYTE:            return(TO_LANG(S_mus_ubyte));
    case MUS_BFLOAT:           return(TO_LANG(S_mus_bfloat));
    case MUS_LFLOAT:           return(TO_LANG(S_mus_lfloat));
    case MUS_BINT:             return(TO_LANG(S_mus_bint));
    case MUS_LINT:             return(TO_LANG(S_mus_lint));
    case MUS_BINTN:            return(TO_LANG(S_mus_bintn));
    case MUS_LINTN:            return(TO_LANG(S_mus_lintn));
    case MUS_B24INT:           return(TO_LANG(S_mus_b24int));
    case MUS_L24INT:           return(TO_LANG(S_mus_l24int));
    case MUS_BDOUBLE:          return(TO_LANG(S_mus_bdouble));
    case MUS_LDOUBLE:          return(TO_LANG(S_mus_ldouble));
    case MUS_UBSHORT:          return(TO_LANG(S_mus_ubshort));
    case MUS_ULSHORT:          return(TO_LANG(S_mus_ulshort));
    case MUS_BDOUBLE_UNSCALED: return(TO_LANG(S_mus_bdouble_unscaled));
    case MUS_LDOUBLE_UNSCALED: return(TO_LANG(S_mus_ldouble_unscaled));
    case MUS_BFLOAT_UNSCALED:  return(TO_LANG(S_mus_bfloat_unscaled));
    case MUS_LFLOAT_UNSCALED:  return(TO_LANG(S_mus_lfloat_unscaled));
    }
  return(NULL);
}

static const char *any_data_format_name(int sndlib_format)
{
  if (MUS_DATA_FORMAT_OK(sndlib_format))
    return(mus_data_format_name(sndlib_format));
  else return(mus_header_original_format_name(mus_header_original_format(),
					      mus_header_type()));
}

#define SEEK_FILE_LENGTH(File) lseek(File, 0L, SEEK_END)
static int read_bicsf_header(const char *filename, int chan);


/* ------------------------------------ NeXT (or Sun) -------------------------------- 
 * 
 *   0:  ".snd"
 *   4:  data_location (bytes) (not necessarily word aligned on Sun)
 *   8:  data_size (bytes) -- sometimes incorrect ("advisory")
 *   12: data format indicator -- see below
 *   16: srate (int)
 *   20: chans
 *   24: comment start
 *   
 * in an AFsp file, the first 4 bytes of the comment are "AFsp",
 * for bicsf, the integer at 28 is 107364 or 107415
 *
 * on NeXTStep, always big-endian.  ".snd"==0x2e736e64 on big-endian machines.
 *
 * formats are: 
 * 0 unspecified, 1 mulaw_8, 2 linear_8, 3 linear_16, 4 linear_24, 5 linear_32, 6 float,
 * 7 double, 8 indirect, 9 nested, 10 dsp_core, 11 dsp_data_8, 12 dsp_data_16, 13 dsp_data_24,
 * 14 dsp_data_32, 16 display, 17 mulaw_squelch, 18 emphasized, 19 compressed, 20 compressed_emphasized
 * 21 dsp_commands, 22 dsp_commands_samples, 23 adpcm_g721, 24 adpcm_g722, 25 adpcm_g723,
 * 26 adpcm_g723_5, 27 alaw_8, 28 aes, 29 delat_mulaw_8 
 *   internal Snd(lib)-only formats: 
 *     30: mus_lint, 31: mus_lfloat, 
 *     32: mus_bintn, 33: mus_lintn,
 *     34: mus_ldouble and others... (added by me for Snd internal use)
 */

/* according to the file /usr/share/magic, the DECN versions were little endian */

static int read_next_header(const char *filename, int chan)
{
  int maybe_bicsf, err = MUS_NO_ERROR, i;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
  data_location = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
  if (data_location < 24) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data location: " OFF_TD "?", filename, data_location));
  data_size = mus_char_to_bint((unsigned char *)(hdrbuf + 8));
  /* can be bogus -- fixup if possible */
  true_file_length = SEEK_FILE_LENGTH(chan);
  if ((data_size <= 24) || (data_size > true_file_length))
    data_size = (true_file_length - data_location);
  else
    {
      if (true_file_length > (off_t)(1 << 31))
	data_size = true_file_length - data_location; /* assume size field overflowed 32 bits */
    }
  original_data_format = mus_char_to_bint((unsigned char *)(hdrbuf + 12));
  switch (original_data_format) 
    {
    case 1:  data_format = MUS_MULAW;            break;
    case 2:  data_format = MUS_BYTE;             break; /* some sound files assume MUS_UBYTE here! (NAS from 1994 X11R6 contrib) */
    case 3:  data_format = MUS_BSHORT;           break;
    case 4:  data_format = MUS_B24INT;           break;
    case 5:  data_format = MUS_BINT;             break;
    case 6:  data_format = MUS_BFLOAT;           break;
    case 7:  data_format = MUS_BDOUBLE;          break;
    case 18: data_format = MUS_BSHORT;           break; /* "emphasized": Xavier Serra's de-emphasis filter: y(n) = x(n) + .9 y(n-1) */
    case 27: data_format = MUS_ALAW;             break;
    case 30: data_format = MUS_LINT;             break; /* from here on, for Snd's internal benefit -- these are probably not used elsewhere */
    case 31: data_format = MUS_LFLOAT;           break; 
    case 32: data_format = MUS_BINTN;            break; 
    case 33: data_format = MUS_LINTN;            break; 
    case 34: data_format = MUS_LDOUBLE;          break; 
    case 35: data_format = MUS_ULSHORT;          break; 
    case 36: data_format = MUS_UBSHORT;          break; 
    case 37: data_format = MUS_LFLOAT_UNSCALED;  break;
    case 38: data_format = MUS_BFLOAT_UNSCALED;  break;
    case 39: data_format = MUS_LDOUBLE_UNSCALED; break;
    case 40: data_format = MUS_BDOUBLE_UNSCALED; break;
    case 41: data_format = MUS_LSHORT;           break; 
    case 42: data_format = MUS_L24INT;           break; 
    case 43: data_format = MUS_UBYTE;            break; 
    default: data_format = MUS_UNKNOWN;          break;
    }
  srate = mus_char_to_bint((unsigned char *)(hdrbuf + 16));
  chans = mus_char_to_bint((unsigned char *)(hdrbuf + 20));
  comment_start = 0;
  comment_end = 0;
  for (i = 24; i < data_location - 1; i++)
    if (hdrbuf[i] == '\0') 
      break;
    else
      {
	if (hdrbuf[i] != ' ')
	  {
	    comment_start = i;
	    comment_end = data_location - 1;
	    break;
	  }
      }
  if (comment_end < comment_start) comment_end = comment_start;
  maybe_bicsf = mus_char_to_bint((unsigned char *)(hdrbuf + 28));
  if (maybe_bicsf == 107364) err = read_bicsf_header(filename, chan);
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(err);
}

static int sndlib_format_to_next(int format)
{
  switch (format)
    {
    case MUS_MULAW:            return(1);  break;
    case MUS_BYTE:             return(2);  break;
    case MUS_BSHORT:           return(3);  break;
    case MUS_B24INT:           return(4);  break;
    case MUS_BINT:             return(5);  break;
    case MUS_BFLOAT:           return(6);  break;
    case MUS_BDOUBLE:          return(7);  break;
    case MUS_ALAW:             return(27); break;
    case MUS_LINT:             return(30); break; /* see above */
    case MUS_LFLOAT:           return(31); break; 
    case MUS_BINTN:            return(32); break; 
    case MUS_LINTN:            return(33); break; 
    case MUS_LDOUBLE:          return(34); break; 
    case MUS_ULSHORT:          return(35); break; 
    case MUS_UBSHORT:          return(36); break;
    case MUS_LFLOAT_UNSCALED:  return(37); break;
    case MUS_BFLOAT_UNSCALED:  return(38); break;
    case MUS_LDOUBLE_UNSCALED: return(39); break;
    case MUS_BDOUBLE_UNSCALED: return(40); break;
    case MUS_LSHORT:           return(41); break;
    case MUS_L24INT:           return(42); break;
    case MUS_UBYTE:            return(43); break;
    default: 
      return(mus_error(MUS_UNSUPPORTED_DATA_FORMAT, "Next header: can't write data format: %d (%s)",
		       format,
		       any_data_format_name(format)));
      break;
    }
}

#if MUS_DEBUGGING
  #define CHK_WRITE(Fd, Buf, Len)						\
    do { size_t bytes = 0;							\
         if (((Len) > 0) && ((bytes = write(Fd, Buf, Len)) == 0))	\
	   fprintf(stderr, "%s[%d]: header write error (wrote: %d != requested: %d)", c__FUNCTION__, __LINE__, (int)bytes, (int)(Len)); \
       } while (0)
  #define CHK_READ(Fd, Buf, Len)			  \
    do { size_t bytes = 0;							\
         if (((Len) > 0) && ((bytes = read(Fd, Buf, (Len))) == 0))	\
  	   fprintf(stderr, "%s[%d]: header read error (read %d != requested %d)", c__FUNCTION__, __LINE__, (int)bytes, (int)(Len)); \
       } while (0)
#else
  #define CHK_WRITE(Fd, Buf, Len) do {if (((Len) > 0) && (write(Fd, Buf, Len) == 0)) fprintf(stderr, "header write error");} while (0)
  #define CHK_READ(Fd, Buf, Len) do {if (((Len) > 0) && (read(Fd, Buf, Len) == 0)) fprintf(stderr, "header read error");} while (0)
#endif

static void write_next_comment(int fd, const char *comment, int len, int loc)
{
  if (len > 0)
    CHK_WRITE(fd, (unsigned char *)comment, len);
  len = loc - (len + 24);
  if (len > 0)
    {
      unsigned char *combuf;
      combuf = (unsigned char *)CALLOC(len, sizeof(char));
      CHK_WRITE(fd, combuf, len);
      FREE(combuf);
    }
}

int mus_header_write_next_header(int chan, int wsrate, int wchans, int loc, int siz, int format, const char *comment, int len)
{
  int i, j;
  write_four_chars((unsigned char *)hdrbuf, I_DSND); /* ".snd" */
  i = len / 4;
  j = 24 + (4 * (i + 1));
  if (loc < j) loc = j;
  mus_bint_to_char((unsigned char *)(hdrbuf + 4), loc);
  mus_bint_to_char((unsigned char *)(hdrbuf + 8), siz);
  mus_bint_to_char((unsigned char *)(hdrbuf + 12), sndlib_format_to_next(format));
  mus_bint_to_char((unsigned char *)(hdrbuf + 16), wsrate);
  mus_bint_to_char((unsigned char *)(hdrbuf + 20), wchans);
  CHK_WRITE(chan, hdrbuf, 24);
  write_next_comment(chan, comment, len, loc);
  data_location = loc;
  return(MUS_NO_ERROR);
}



/* ------------------------------------ AIFF ------------------------------------ 
 *
 *  0: "FORM"
 *  4: size (bytes)
 *  8: "AIFF" or "AIFC" -- the latter includes compressed formats (list extended for 8.5 Sound.h)
 *
 *  Thereafter the file is organized into "chunks", each chunk being 
 *  a 4-byte identifer followed by an int (4-bytes) giving the chunk size
 *  not including the 8-byte header.  AIFF data is signed.  If the chunk
 *  size is odd, an extra (unaccounted-for) null byte is added at the end.
 *
 *  The chunks we want are "COMM", "SSND", and "APPL".
 *
 * COMM: 0: chans
 *       2: frames
 *       6: bits per sample
 *       8: srate as 80-bit IEEE float
 *  then if AIFC (not AIFF), 4 bytes giving compression id ("NONE"=not compressed)
 *    followed by Pascal string giving long name of compression type
 *
 * SSND: 0: data location (offset within SSND chunk)
 *
 * Other chunks include:  ANNO: a comment, INST: loop control, MARK: marker, MIDI: midi,
 *                        COMT: comment (max 65536 chars), NAME: sound name, AUTH: author's name
 *                        (c), AESD: recording data, APPL: application specific stuff
 *    "MARK" size short-#marks {marks} -- latter are short-ID long-position pstring-name.
 *    "INST" size chars[baseNote detune lowNote highNote lowVelocity HighVelocity] short-gain loops[sustain release]
 *      loop: short-playMode marker-begin marker-end (signed?) shorts)
 *         playMode: 0 no loop, 1 forward loop, 2 forward/backward loop
 *      chars are MIDI data (detune is in cents)
 *    "MIDI" size MIDI-data...
 *    "AESD" size AES Channel Status Data (24 bytes as specified by AES)
 *      see "AES: Guidelines for the use of the AES3 interface"
 *      byte 0: bit 0: 0 = consumer, 1 = pro
 *              bit 1: 0 = audio, 1 = non-audio
 *              bits 2:4: emphasis: 0:none, 4:none, 6:CD, 7:CCITT J17
 *              bits 6:7: srate: 00 = 48KHz, 01 = 48, 10 = 44.1, 11 = 32
 *      byte 1: bits 0:3: chans: 2:mono, else stereo
 *      byte 2 for word size stuff (always ends up 16-bit): bits 3-5 = sample length where 4 = 16-bit
 *      byte 3: multi-channels modes, 4: AES sync ref, 5:unused, 6-9:ASCII source ID, 10-13:ASCII destination ID
 *      byte 14-17:local sample addr, 18-21:time of day addr, then CRC checks
 *    "APPL" size signature data
 *    "COMT" size short-#comments {comments} -- the latter are long-time marker short-text-length char-text
 *       time is in seconds since 1-Jan-1904
 *    "NAME"/"AUTH"/"(c) "/"ANNO" size char-name
 *    "FVER" size(4) AIFC-format-version -- currently always 0xA2805140
 *    "SAXL" -- a desperate kludge to get around Apple's own compression schemes!
 *
 * always big-endian
 * There was also (briefly) an AIFS file, now deprecated.
 */

/* ieee-80 conversions -- design by committee! */
/* this code taken from CSound sources -- apparently originally written by Malcolm Slaney at Apple */

#define ULPOW2TO31	((unsigned int)0x80000000)
#define DPOW2TO31	((double)2147483648.0)	/* 2^31 */

static double myUlongToDouble(unsigned int ul)
{
  double val;
  if(ul & ULPOW2TO31) val = DPOW2TO31 + (ul & (~ULPOW2TO31));
  else val = ul;
  return val;
}

static unsigned int myDoubleToUlong(double val)
{
  unsigned int ul;
  if(val < DPOW2TO31) ul = (unsigned int)val;
  else ul = ULPOW2TO31 | (unsigned int)(val-DPOW2TO31);
  return ul;
}

static double ieee_80_to_double(unsigned char *p)
{
  unsigned char sign;
  short lexp = 0;
  unsigned int mant1 = 0;
  unsigned int mant0 = 0;
  lexp = *p++;  lexp <<= 8;  lexp |= *p++;  sign = (lexp & 0x8000) ? 1 : 0;  lexp &= 0x7FFF;
  mant1 = *p++;  mant1 <<= 8;  mant1 |= *p++;  mant1 <<= 8;  mant1 |= *p++;  mant1 <<= 8;  mant1 |= *p++;
  mant0 = *p++;  mant0 <<= 8;  mant0 |= *p++;  mant0 <<= 8;  mant0 |= *p++;  mant0 <<= 8;  mant0 |= *p++;
  if(mant1 == 0 && mant0 == 0 && lexp == 0 && sign == 0)
    return 0.0;
  else
    {
      double val;
      val = myUlongToDouble(mant0) * pow(2.0, -63.0);
      val += myUlongToDouble(mant1) * pow(2.0, -31.0);
      val *= pow(2.0, ((double) lexp) - 16383.0);
      return sign ? -val : val;
    }
}

static void double_to_ieee_80(double val, unsigned char *p)
{
  short lexp = 0;
  unsigned char sign = 0;
  unsigned int mant1 = 0;
  unsigned int mant0 = 0;
  if(val < 0.0)	{  sign = 1;  val = -val; }
  if(val != 0.0)	/* val identically zero -> all elements zero */
    {
      lexp = (short)(log(val) / log(2.0) + 16383.0);
      val *= pow(2.0, 31.0 + 16383.0 - (double)lexp);
      mant1 = myDoubleToUlong(val);
      val -= myUlongToDouble(mant1);
      val *= pow(2.0, 32.0);
      mant0 = myDoubleToUlong(val);
    }
  *p++ = ((sign << 7) | (lexp >> 8));  *p++ = 0xFF & lexp;  
  *p++ = 0xFF & (mant1 >> 24);  *p++ = 0xFF & (mant1 >> 16);  *p++ = 0xFF & (mant1 >> 8);  *p++ = 0xFF & (mant1);
  *p++ = 0xFF & (mant0 >> 24);  *p++ = 0xFF & (mant0 >> 16);  *p++ = 0xFF & (mant0 >> 8);  *p++ = 0xFF & (mant0);
}


static off_t update_form_size, update_frames_location, update_ssnd_location;

static int seek_and_read(int chan, unsigned char *buf, off_t offset, int nbytes)
{
  if (offset < 0) return(-1);
  lseek(chan, offset, SEEK_SET);
  return(read(chan, buf, nbytes));
}

static int read_aiff_marker(int m, unsigned char *buf)
{
  int psize;
  marker_ids[m] = mus_char_to_bshort((unsigned char *)buf);
  marker_positions[m] = mus_char_to_bint((unsigned char *)(buf + 2));
  psize = (int)buf[6] + 1; 
  if (psize & 1) psize++; 
  return(psize+6);
}

static int read_aiff_header(const char *filename, int chan, int overall_offset)
{
  /* we know we have checked for FORM xxxx AIFF|AIFC when we arrive here */
  /* as far as I can tell, the COMM block has the header data we seek, and the SSND block has the sound data */
  int chunksize, chunkloc, i, j, ssnd_bytes = 0;
  bool happy, got_comm = false;
  off_t offset;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 8 + overall_offset));
  update_ssnd_location = 0;
  chunkloc = 12 + overall_offset;
  offset = 0;
  for (i = 0; i < AUX_COMMENTS; i++) aux_comment_start[i] = 0;
  data_format = MUS_BSHORT;
  srate = 0;
  chans = 0;
  happy = true;
  true_file_length = SEEK_FILE_LENGTH(chan);
  update_form_size = mus_char_to_bint((unsigned char *)(hdrbuf + 4 + overall_offset)); /* should be file-size-8 unless there are multiple forms */
  while (happy)
    {
      offset += chunkloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	{
	  if ((got_comm) && (data_location > 0))
	    {
	      mus_print("%s, aiff header: chunks confused at " OFF_TD "; will try to continue", filename, offset);
	      break;
	    }
	  return(mus_error(MUS_HEADER_READ_FAILED, "%s, aiff header: chunks confused at " OFF_TD , filename, offset));
	}
      chunksize = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
      if ((chunksize == 0) && /* can be empty data chunk */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      /* fprintf(stderr,"chunk: %c%c%c%c for %d\n", hdrbuf[0], hdrbuf[1], hdrbuf[2], hdrbuf[3], chunksize); */
      if (match_four_chars((unsigned char *)hdrbuf, I_COMM))
	{
	  int frames;
	  got_comm = true;
	  chans = mus_char_to_bshort((unsigned char *)(hdrbuf + 8));
	  frames = mus_char_to_ubint((unsigned char *)(hdrbuf + 10)); /* was bint 27-Jul-01 */
	  update_frames_location = 10 + offset;
	  original_data_format = mus_char_to_bshort((unsigned char *)(hdrbuf + 14));
	  if ((original_data_format % 8) != 0) 
	    {
	      /* weird sizes are legal --
	       * these samples are left-justified (and zero padded on the right), so
	       * we can handle any bit size by rounding up to the nearest byte.
	       */
	      original_data_format = 8 * (1 + (original_data_format >> 3));
	    }
	  if (original_data_format == 8) data_format = MUS_BYTE;
	  else if (original_data_format == 16) data_format = MUS_BSHORT;
	  else if (original_data_format == 24) data_format = MUS_B24INT;
	  else if (original_data_format == 32) data_format = MUS_BINT;
	  else if (original_data_format == 64) data_format = MUS_BDOUBLE;
	  else return(mus_error(MUS_HEADER_READ_FAILED, "%s: bits per sample: %d?", filename, mus_char_to_bshort((unsigned char *)(hdrbuf + 14))));
	  srate = (int)ieee_80_to_double((unsigned char *)(hdrbuf + 16));
	  /* if AIFC, compression type over-rides (possibly bogus) original_data_format */
	  if (type_specifier == mus_char_to_uninterpreted_int((unsigned const char *)I_AIFC))
	    {
	      /* some aifc files assume the compression field is a new and very weird chunk!! -- surely a bug? */
	      /* AIFF spec says COMM size is always 18, but this is amended in the newer AIFC spec */
	      if (chunksize == 18) chunksize += (5 + ((int)hdrbuf[30]));             /* 5 = chunk header length in this case */
	      if ((!(match_four_chars((unsigned char *)(hdrbuf + 26), I_NONE))) &&
		  (!(match_four_chars((unsigned char *)(hdrbuf + 26), I_twos))))
		{
		  original_data_format = mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 26));
		  if ((match_four_chars((unsigned char *)(hdrbuf + 26), I_ALAW)) || 
		      (match_four_chars((unsigned char *)(hdrbuf + 26), I_alaw)))
		    data_format = MUS_ALAW;
		  else 
		    {
		      if ((match_four_chars((unsigned char *)(hdrbuf + 26), I_ULAW)) ||
			  (match_four_chars((unsigned char *)(hdrbuf + 26), I_ulaw)))
			data_format = MUS_MULAW;
		      else 
			{
			  if ((match_four_chars((unsigned char *)(hdrbuf + 26), I_sowt)) ||
			      (match_four_chars((unsigned char *)(hdrbuf + 26), I_ni23)))
			    {
			      /* Sound.h sez sowt is just 16-bit format */
			      if (data_format == MUS_BSHORT) data_format = MUS_LSHORT;
			      else if (data_format == MUS_B24INT) data_format = MUS_L24INT;
			      else if (data_format == MUS_BINT) data_format = MUS_LINT;
			    }
			  else
			    {
			      if (match_four_chars((unsigned char *)(hdrbuf + 26), I_raw_))
				{
				  if (data_format == MUS_BYTE) data_format = MUS_UBYTE;
				  else if (data_format == MUS_BSHORT) data_format = MUS_UBSHORT;
				}
			      else
				{
				  if ((match_four_chars((unsigned char *)(hdrbuf + 26), I_fl32)) ||
				      (match_four_chars((unsigned char *)(hdrbuf + 26), I_FL32)))
				    data_format = MUS_BFLOAT;
				  else
				    {
				      if (match_four_chars((unsigned char *)(hdrbuf + 26), I_fl64))
					data_format = MUS_BDOUBLE;
				      else
					{
					  if (match_four_chars((unsigned char *)(hdrbuf + 26), I_ima4))
					    {
					      block_align = 34;
					      original_data_format = MUS_AIFF_IMA_ADPCM;
					    }
					  else
					    {
					      if (match_four_chars((unsigned char *)(hdrbuf + 26), I_in32))
						data_format = MUS_BINT;
					      else
						{
						  if (match_four_chars((unsigned char *)(hdrbuf + 26), I_in24))
						    data_format = MUS_B24INT;
						  else
						    {
						      /* others from Sound.h:
							 0x6D730002, -- Microsoft ADPCM - ACM code 2
							 0x6D730011, -- DVI/Intel IMA ADPCM - ACM code 17
							 'MAC3' -- MACE 3:1
							 'MAC6' -- MACE 6:1
							 'cdx4' -- CD/XA 4:1
							 'cdx2' -- CD/XA 2:1
							 'dvca' -- DV Audio
							 'QDMC' -- QDesign music
							 'QDM2' -- QDesign2 music
							 'Qclp' -- QUALCOMM PureVoice
							 0x6D730055 -- MPEG Layer 3, CBR only (pre QT4.1)
							 '.mp3' -- MPEG Layer 3, CBR & VBR (QT4.1 and later)
						      */
						      data_format = MUS_UNKNOWN;
						    }
						}
					    }
					}
				    }
				}
			    }
			}
		    }
		}
	    }
	  data_size = (frames * mus_bytes_per_sample(data_format) * chans);
	}
      else
	{
	  if (match_four_chars((unsigned char *)hdrbuf, I_SSND))
	    {
	      if (data_location != 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: two SSND chunks found", filename));
	      update_ssnd_location = offset + 4;
	      data_location = mus_char_to_bint((unsigned char *)(hdrbuf + 8)) + offset + 16; /* Baroque! */
	      /* offset is where the hdrbuf is positioned in the file, the sound data offset itself is at loc+8 and the */
	      /* 0-based location of the sound data is at the end of the chunk = 16 (8 = header+4 = offset+4 = blocksize) */
	      /* the next int can be the block size if the data is block-aligned */
	      /* only one SSND per AIFF is allowed */
	      if (chunksize == 0) break; /* this may happen while pre-reading an in-progress output file for updating */
	      ssnd_bytes = offset + chunksize - data_location + 8;
	    }
	  else
	    {
	      if ((match_four_chars((unsigned char *)hdrbuf, I_ANNO)) || 
		  (match_four_chars((unsigned char *)hdrbuf, I_COMT)) ||
		  (match_four_chars((unsigned char *)hdrbuf, I_NAME)) ||
		  (match_four_chars((unsigned char *)hdrbuf, I_AUTH)))
		{
		  j = 0;
		  for (i = 0; i < AUX_COMMENTS; i++) 
		    if (aux_comment_start[i] == 0) 
		      {
			j = i; 
			break;
		      }
		  if (j >= AUX_COMMENTS) 
		    {
		      mus_print("read_aiff_header: ran out of auxiliary comment space");
		      j = 0;
		    }
		  aux_comment_start[j] = offset + 8;
		  if (match_four_chars((unsigned char *)hdrbuf, I_COMT)) 
		    aux_comment_start[j] += 8; /* skip time stamp and markerId (not ID, I assume!) */
		  aux_comment_end[j] = offset + 7 + chunksize;
		}
	      else
		{
		  if (match_four_chars((unsigned char *)hdrbuf, I_APPL))
		    {
		      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_MUS_))
			{
			  /* my own chunk has the arbitrary length comment I use (actually the ASCII    */
			  /* representation of a lisp program evaluated in the CLM package) to handle mix et al. */
			  /* It is nothing more than the actual string -- remember to pad to even length here. */
			  comment_start = offset + 12;
			  comment_end = comment_start + chunksize - 5;
			}
		      else 
			{
			  if ((match_four_chars((unsigned char *)(hdrbuf + 8), I_SU7M)) ||
			      (match_four_chars((unsigned char *)(hdrbuf + 8), I_SU7R)))
			    {
			      mus_print("this is an SU700 ssp file?");
			      data_location = 512;
			      chans = 1;
			      /* actually SU7M and SU7R point to 2 chan data as separate chunks */
			    }
			}
		    }
		  else
		    {
		      if (match_four_chars((unsigned char *)hdrbuf, I_INST))
			{
			  base_note = hdrbuf[8];
			  base_detune = hdrbuf[9];
			  loop_modes[0] = mus_char_to_bshort((unsigned char *)(hdrbuf + 16));
			  loop_starts[0] = mus_char_to_bshort((unsigned char *)(hdrbuf + 18));
			  loop_ends[0] = mus_char_to_bshort((unsigned char *)(hdrbuf + 20));
			  loop_modes[1] = mus_char_to_bshort((unsigned char *)(hdrbuf + 22));
			  loop_starts[1] = mus_char_to_bshort((unsigned char *)(hdrbuf + 24));
			  loop_ends[1] = mus_char_to_bshort((unsigned char *)(hdrbuf + 26));
			  /* these are mark numbers */
			}
		      else
			{
			  if (match_four_chars((unsigned char *)hdrbuf, I_MARK))
			    {
			      int num_marks, m, moff, msize;
			      /* unsigned short #marks, each mark: id pos name (pstring damn it) */
			      num_marks = mus_char_to_ubshort((unsigned char *)(hdrbuf + 8));
			      if (num_marks > markers)
				{
				  if (markers > 0)
				    {
				      if (marker_ids) FREE(marker_ids); 
				      if (marker_positions) FREE(marker_positions);
				    }
				  markers = num_marks;
				  marker_ids = (int *)CALLOC(markers, sizeof(int));
				  marker_positions = (int *)CALLOC(markers, sizeof(int));
				}
			      moff = 10;
			      for (m = 0; m < num_marks; m++)
				{
				  if (seek_and_read(chan, (unsigned char *)hdrbuf, offset + moff, 8) > 0)
				    {
				      msize = read_aiff_marker(m, (unsigned char *)hdrbuf);
				      moff += msize;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
      if ((offset + chunkloc) >= update_form_size) happy = false;
    }
  if (!got_comm)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no COMM chunk", filename));
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no SSND (data) chunk", filename));
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  if ((data_size > ssnd_bytes) && (data_format != MUS_UNKNOWN))
    data_size = ssnd_bytes;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

static int sndlib_format_to_aiff_bits(int format)
{
  switch (format)
    {
    case MUS_BSHORT: case MUS_LSHORT: case MUS_UBSHORT: case MUS_ULSHORT:  return(16); break;
    case MUS_B24INT: case MUS_L24INT:                                      return(24); break;
    case MUS_BINT: case MUS_LINT: case MUS_BFLOAT: case MUS_LFLOAT:        return(32); break;
    case MUS_BDOUBLE: case MUS_LDOUBLE:                                    return(64); break;
    case MUS_BYTE: case MUS_UBYTE: case MUS_MULAW: case MUS_ALAW:          return(8);  break;
    default: 
      return(mus_error(MUS_UNSUPPORTED_DATA_FORMAT, "aiff header: can't write data format: %d (%s)",
		       format,
		       any_data_format_name(format)));
      break;
    }
}

static const char *sndlib_format_to_aifc_name(int format)
{
  switch (format)
    {
    case MUS_BSHORT: case MUS_B24INT: case MUS_BINT: case MUS_BYTE: return((const char *)I_NONE); break; /* use in24 and in32? */
    case MUS_LSHORT: case MUS_L24INT: case MUS_LINT:                return((const char *)I_sowt); break; /* should this use ni23? */
    case MUS_BFLOAT:                                                return((const char *)I_fl32); break;
    case MUS_BDOUBLE:                                               return((const char *)I_fl64); break;
    case MUS_UBYTE: case MUS_UBSHORT:                               return((const char *)I_raw_); break;
    case MUS_MULAW:                                                 return((const char *)I_ulaw); break;
    case MUS_ALAW:                                                  return((const char *)I_alaw); break;
    default:                                                        return((const char *)I_NONE); break;
    }
}

static int write_aif_header(int chan, int wsrate, int wchans, int siz, int format, const char *comment, int len, bool aifc_header)
{
  /* we write the simplest possible AIFC header: AIFC | COMM | APPL-MUS_ if needed | SSND eof. */
  /* the assumption being that we're going to be appending sound data once the header is out   */
  /* INST and MARK chunks added Jul-95 for various programs that expect them (MixView).        */
  int i, j, lenhdr, lenloop, curend, extra; /* set aifc to 0 to get old-style AIFF header */
  char *str;
  lenhdr = 0;
  extra = 0;
  curend = 0;
  lenloop = 38;
  if ((loop_modes[0] != 0) || (loop_modes[1] != 0)) lenloop = 42 + 28;
  if (len != 0) 
    {
      lenhdr = 12;
      if ((len % 4) != 0)
	extra = (4 - (len % 4));
    }
  write_four_chars((unsigned char *)hdrbuf, I_FORM);
  if (aifc_header) 
    mus_bint_to_char((unsigned char *)(hdrbuf + 4), len + 30 + 16 + lenloop + siz + lenhdr + extra + 12 + 10);
  else mus_bint_to_char((unsigned char *)(hdrbuf + 4), len + 30 + 16 + lenloop + siz + lenhdr + extra);
  /* 
   * comment length + 4 for AIFF 18+8 for I_COMM info + 16 for I_SSND info + 38 for INST and MARK +
   * siz for data + 12 for comment header if any + padding == total size - 8 (i.e. FORM header).   
   * INST+MARK (38) added 3-Jul-95 for Notam software compatibility 
   */
  if (aifc_header) 
    {
      write_four_chars((unsigned char *)(hdrbuf + 8), I_AIFC); 
      CHK_WRITE(chan, hdrbuf, 12);
      curend = 12;
      write_four_chars((unsigned char *)hdrbuf, I_FVER);
      mus_bint_to_char((unsigned char *)(hdrbuf + 4), 4);
      mus_bint_to_char((unsigned char *)(hdrbuf + 8), 0xA2805140);
    }
  else write_four_chars((unsigned char *)(hdrbuf + 8), I_AIFF);
  write_four_chars((unsigned char *)(hdrbuf + 12), I_COMM);
  if (aifc_header) 
    mus_bint_to_char((unsigned char *)(hdrbuf + 16), 18 + 10); 
  else mus_bint_to_char((unsigned char *)(hdrbuf + 16), 18);
  mus_bshort_to_char((unsigned char *)(hdrbuf + 20), (short)wchans);
  if (wchans > 0)
    mus_bint_to_char((unsigned char *)(hdrbuf + 22), siz / (wchans * mus_bytes_per_sample(format)));
  mus_bshort_to_char((unsigned char *)(hdrbuf + 26), sndlib_format_to_aiff_bits(format));
  double_to_ieee_80((double)wsrate, (unsigned char *)(hdrbuf + 28));
  if (aifc_header)
    {
      str = (char *)sndlib_format_to_aifc_name(format);
      write_four_chars((unsigned char *)(hdrbuf + 38), (const unsigned char *)str);
      (*(unsigned char *)(hdrbuf + 42)) = 4; /* final pad null not accounted-for */
      write_four_chars((unsigned char *)(hdrbuf + 43), (const unsigned char *)str);
      (*(unsigned char *)(hdrbuf + 47)) = 0;
      i = 48;
    }
  else i = 38;
  if (len != 0)
    {
      if (aifc_header)
	{
	  write_four_chars((unsigned char *)(hdrbuf + 48), I_APPL);
	  mus_bint_to_char((unsigned char *)(hdrbuf + 52), len + 4 + extra);
	  write_four_chars((unsigned char *)(hdrbuf + 56), I_MUS_);
	  i = 60;
	}
      else
	{
	  write_four_chars((unsigned char *)(hdrbuf + 38), I_APPL);
	  mus_bint_to_char((unsigned char *)(hdrbuf + 42), len + 4 + extra);
	  write_four_chars((unsigned char *)(hdrbuf + 46), I_MUS_);
	  i = 50;
	}
      for (j = 0; j < len; j++)
	{
	  if (i == HDRBUFSIZ)
	    {
	      curend += HDRBUFSIZ;
	      CHK_WRITE(chan, hdrbuf, HDRBUFSIZ);
	      i = 0;
	    }
	  hdrbuf[i] = comment[j];
	  i++;
	}
      if (extra != 0)
	{
	  if ((i + extra) > HDRBUFSIZ)
	    {
	      curend += i;
	      CHK_WRITE(chan, hdrbuf, i);
	      i = 0;
	    }
	  for (j = 0; j < extra; j++)
	    {
	      hdrbuf[i] = 0;
	      i++;
	    }
	}
    }
  curend += i;
  CHK_WRITE(chan, hdrbuf, i);
  if ((loop_modes[0] == 0) && (loop_modes[1] == 0))
    {
      write_four_chars((unsigned char *)hdrbuf, I_MARK);   /* SoundHack includes a blank MARK chunk for some reason */
      mus_bint_to_char((unsigned char *)(hdrbuf + 4), 2);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 8), 0);
      write_four_chars((unsigned char *)(hdrbuf + 10), I_INST);
      mus_bint_to_char((unsigned char *)(hdrbuf + 14), 20);
      mus_bint_to_char((unsigned char *)(hdrbuf + 18), 0x3c00007f); /* base-note = middle C, detune = 0, lownote = 0, highnote = 0x7f */
      mus_bint_to_char((unsigned char *)(hdrbuf + 22), 0x017f0000); /* lowvelocity = 1, highvelocity = 0x7f, gain = 0 */
      mus_bint_to_char((unsigned char *)(hdrbuf + 26), 0);          /* no loops */
      mus_bint_to_char((unsigned char *)(hdrbuf + 30), 0); 
      mus_bint_to_char((unsigned char *)(hdrbuf + 34), 0);
      CHK_WRITE(chan, hdrbuf, 38);
      curend += 38;
    }
  else
    {
      write_four_chars((unsigned char *)hdrbuf, I_MARK); 
      mus_bint_to_char((unsigned char *)(hdrbuf + 4), 8 * 4 + 2); /* 2 for mark#, then 2:id + 4:pos + 2:pstr */
      /* loop_info: 0..3 are markers positions (ids 1..4) */
      mus_bshort_to_char((unsigned char *)(hdrbuf + 8), 4);
      for (j = 0; j < 4; j++)
	{
	  mus_bshort_to_char((unsigned char *)(hdrbuf + 10 + 8 * j), j + 1);
	  switch (j)
	    {
	    case 0: mus_bint_to_char((unsigned char *)(hdrbuf + 10 + 8 * j + 2), loop_starts[0]); break;
	    case 1: mus_bint_to_char((unsigned char *)(hdrbuf + 10 + 8 * j + 2), loop_ends[0]);   break;
	    case 2: mus_bint_to_char((unsigned char *)(hdrbuf + 10 + 8 * j + 2), loop_starts[1]); break;
	    case 3: mus_bint_to_char((unsigned char *)(hdrbuf + 10 + 8 * j + 2), loop_ends[1]);   break;
	    }
	  mus_bshort_to_char((unsigned char *)(hdrbuf + 10 + 8 * j + 6), 0);
	}
      CHK_WRITE(chan, hdrbuf, 42);
      curend += 42;
      write_four_chars((unsigned char *)hdrbuf, I_INST); 
      mus_bint_to_char((unsigned char *)(hdrbuf + 4), 20);
      mus_bint_to_char((unsigned char *)(hdrbuf + 8), 0x3c00007f);
      mus_bint_to_char((unsigned char *)(hdrbuf + 12), 0x017f0000);
      hdrbuf[8] = (unsigned char)(base_note);
      hdrbuf[9] = (unsigned char)(base_detune);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 16), loop_modes[0]);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 18), 1);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 20), 2);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 22), loop_modes[1]);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 24), 3);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 26), 4);
      CHK_WRITE(chan, hdrbuf, 28);
      curend += 28;
    }
  write_four_chars((unsigned char *)(hdrbuf), I_SSND);
  mus_bint_to_char((unsigned char *)(hdrbuf + 4), siz + 8);
  mus_bint_to_char((unsigned char *)(hdrbuf + 8), 0);                        /* "offset" */
  mus_bint_to_char((unsigned char *)(hdrbuf + 12), 0);                       /* "blocksize " */
  CHK_WRITE(chan, hdrbuf, 16);
  data_location = 16 + curend;
  return(MUS_NO_ERROR);
}

char *mus_header_aiff_aux_comment(const char *name, off_t *starts, off_t *ends)
{
  /* AIFC: look for aux comments (ANNO chunks) */
  char *sc = NULL;
  if ((starts) && (starts[0] != 0))
    {
      off_t full_len;
      int fd, i;
      fd = mus_file_open_read(name);
      if (fd == -1) return(NULL);
      full_len = 0;
      for (i = 0; i < AUX_COMMENTS; i++) 
	if ((starts[i] > 0) && 
	    (starts[i] < ends[i]))
	  full_len += (ends[i] - starts[i] + 3);
      if (full_len > 0)
	{
	  off_t sc_len;
	  sc = (char *)CALLOC(full_len, sizeof(char));
	  sc_len = 0;
	  for (i = 0; i < AUX_COMMENTS; i++) 
	    {
	      off_t start, end, len;
	      start = starts[i];
	      end = ends[i];
	      if ((start > 0) && (start < end))
		{
		  int j;
		  len = end - start + 1;
		  lseek(fd, start, SEEK_SET);
		  CHK_READ(fd, (char *)(sc + sc_len), len);
		  for (j = 0; j < len; j++) 
		    if (sc[j + sc_len] == 0) 
		      sc[j + sc_len] = ' ';
		  sc_len += len;
		  sc[sc_len++] = '\n';
		}
	    }
	}
      CLOSE(fd, name);
    }
  return(sc);
}

/* ------------------------------------ RIFF (wave) ------------------------------------
 *
 * see ftp.microsoft.com:/SoftLib/MSLFILES/MDRK.EXE (also MMSYSTEM.H and MMREG.H)
 *     ftp://ftp.isi.edu/in-notes/rfc2361.txt
 *
 *   0: "RIFF" (little-endian) or "RIFX" (big-endian)
 *   4: size
 *   8: "WAVE"  ("RMID" = midi data, others are AVI, CPPO, ACON, DLS? etc)
 *       AVI chunk can include audio data
 *  
 *   rest very similar to AIFF (odd-sized chunks are padded)
 *
 * fmt  0: format code (see below)
 *      2: chans
 *      4: srate (long)
 *      8: average rate "for buffer estimation"
 *     12: alignment "block size"
 *     14: data size (bits per sample) (PCM only)
 *     16: count (bytes) of extra info in the header (i.e. trailing info added to this basic header)
 *     20: samples per block (short) in dvi_adpcm
 *  
 * formats are: 0: unknown, 1: PCM, 2: ADPCM, 3: IEEE float, 4: VSELP, 5: IBM_CVSD, 6: alaw, 7: mulaw
 *              0x10: OKI_ADPCM, 0x11: DVI_ADPCM, 0x12: MediaSpace_ADPCM,
 *              0x13: Sierra_ADPCM, 0x14: G723_ADPCM, 0x15: DIGISTD, 0x16: DIGIFIX, 0x17: Dialogic ADPCM,
 *              0x18: Mediavision ADPCM, 0x19: HP cu codec, 
 *              0x20: Yamaha_ADPCM, 0x21: SONARC, 0x22: DSPGroup_TrueSpeech
 *              0x23: EchoSC1, 0x24: AudioFile_AF36, 0x25: APTX, 0x26: AudioFile_AF10
 *              0x27: prosody 1612, 0x28: lrc,
 *              0x30: Dolby_Ac2, 0x31: GSM610, 0x32: MSN audio codec, 0x33: Antext_ADPCM, 0x34: Control_res_vqlpc,
 *              0x35: DIGIREAL, 0x36: DIGIADPCM, 0x37: Control_res_cr10, 0x38: NMS_VBXADPCM, 0x39:Roland rdac,
 *              0x3a: echo sc3, 0x3b: Rockwell adpcm, 0x3c: Rockwell digitalk codec, 0x3d: Xebec,
 *              0x40: G721_ADPCM, 0x41: G728 CELP, 0x42: MS G723, 0x50: MPEG, 
 *              0x52: RT24, 0x53: PAC, 0x55: Mpeg layer 3, 0x59: Lucent G723, 0x60: Cirrus,
 *              0x61: ESS Tech pcm, 0x62: voxware (obsolete), 0x63: canopus atrac,
 *              0x64: G726, 0x65: G722, 0x66: DSAT, 0x67: DSAT display,
 *              0x69: voxware (obsolete), 0x70: voxware ac8 (obsolete), 0x71: voxware ac10 (obsolete), 
 *              0x72: voxware ac16 (obsolete), 0x73: voxware ac20 (obsolete), 0x74: voxware rt24, 
 *              0x75: voxware rt29, 0x76: voxware rt29hw (obsolete), 0x77: voxware vr12 (obsolete),
 *              0x78: voxware vr18 (obsolete), 0x79: voxware tq40 (obsolete), 
 *              0x80: softsound, 0x81: voxware tq60 (obsolete), 0x82: MS RT24, 0x83: G729A,
 *              0x84: MVI_MVI2, 0x85: DF G726, 0x86: DF GSM610, 0x88: isaudio, 0x89: onlive,
 *              0x91: sbc24, 0x92: dolby ac3 spdif, 0x97: zyxel adpcm, 0x98: philips lpcbb,
 *              0x99: packed, 0x100: rhetorex adpcm, 
 *              0x101: Irat, 0x102: IBM_alaw?, 0x103: IBM_ADPCM?, 
 *              0x111: vivo G723, 0x112: vivo siren, 0x123: digital g273
 *              0x200: Creative_ADPCM, 0x202: Creative fastspeech 8, 0x203: Creative fastspeech 10, 
 *              0x220: quarterdeck, 0x300: FM_TOWNS_SND, 0x400: BTV digital, 0x680: VME vmpcm,
 *              0x1000: OLIGSM, 0x1001: OLIADPCM, 0x1002: OLICELP, 0x1003: OLISBC, 0x1004: OLIOPR
 *              0x1100: LH codec, 0x1400: Norris, 0x1401: isaudio, 0x1500: Soundspace musicompression, 0x2000: DVM
 * (see http://www.microsoft.com/asf/resources/draft-ietf-fleischman-codec-subtree-00.txt)
 *   and new:   0xFFFE: wave_format_extensible: bits/sample, mapping, 16 byte guid, 1st 2 bytes are code as above
 *
 * RIFF and LIST chunks have nested chunks.  Registered chunk names include:
 *   LIST with subchunks, one of which can be:
 *     INFO itself containing:
 *       IARL: archival location, IART: artist, ICMS: commissioned, ICMT: comment, ICOP: copyright, ICRD: creation date,
 *       ICRP: uh...cropped, IDIM: dimensions, IDPI: dpi, IENG: engineer, IGNR: genre, IKEY: keywords, ILGT: lightness,
 *       IMED: medium, INAM: name, IPLT: palette, IPRD: product, ISBJ: subject, ISFT: software, ISHP: sharpness,
 *       ISRC: source, ISRF: source form, ITCH: technician, ISMP: SMPTE time code, IDIT: digitization time
 *
 * data chunk has the samples
 * other (currently ignored) chunks are wavl = waveform data, fact, cues of some sort, slnt = silence,
 *     plst = playlist, adtl = associated data list, labl = cue label, note = cue comments,
 *     ltxt = text associated with data segment (cue), file, DISP = displayable object,
 *     JUNK = outdated info, PAD = padding, etc
 * fact chunk generally has number of samples (used in compressed files)
 */

static int wave_to_sndlib_format(int osf, int bps, bool little)
{
  switch (osf)
    {
    case 1:
      switch (bps)
	{
	case 8: return(MUS_UBYTE); break;
	case 16: if (little) return(MUS_LSHORT); else return(MUS_BSHORT); break;
	case 32: if (little) return(MUS_LINT); else return(MUS_BINT); break;
	case 24: if (little) return(MUS_L24INT); else return(MUS_B24INT); break;
	default: return(MUS_UBYTE); break;
	}
      break;
    case 3: 
      if (little) 
	{
	  if (bps == 64)
	    return(MUS_LDOUBLE);
	  else return(MUS_LFLOAT); 
	  }
      else 
	{
	  if (bps == 64)
	    return(MUS_BDOUBLE);
	  else return(MUS_BFLOAT); 
	  }
      break;
    case 6: if (bps == 8) return(MUS_ALAW); break;
    case 7: if (bps == 8) return(MUS_MULAW); break;
      /* IBM mulaw follows G711 specs like other versions (this info direct from IBM) */
    case 0x101: return(MUS_MULAW); break;
    case 0x102: return(MUS_ALAW); break;
    }
  return(MUS_UNKNOWN);
}

static int read_riff_header(const char *filename, int chan)
{
  /* we know we have checked for RIFF xxxx WAVE when we arrive here */
  int chunksize, chunkloc, i;
  bool little, got_fmt = false;
  off_t offset;
  little = true;
  if (match_four_chars((unsigned char *)hdrbuf, I_RIFX)) little = false; /* big-endian data in this case, but I've never seen one */
  little_endian = little;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 8));
  chunkloc = 12;
  offset = 0;
  data_format = MUS_UNKNOWN;
  srate = 0;
  chans = 0;
  fact_samples = 0;
  bits_per_sample = 0;
  for (i = 0; i < AUX_COMMENTS; i++) aux_comment_start[i] = 0;
  true_file_length = SEEK_FILE_LENGTH(chan);
  update_form_size = big_or_little_endian_int((unsigned char *)(hdrbuf + 4), little);
  while (true)
    {
      offset += chunkloc;
      if (offset >= true_file_length) break;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 64) <= 0) break;
      chunksize = big_or_little_endian_int((unsigned char *)(hdrbuf + 4), little);
      if ((chunksize == 0) && /* can be empty data chunk */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if (match_four_chars((unsigned char *)hdrbuf, I_fmt_))
	{
	  /*
	   * 8:  short format code        --1 = PCM for example
	   * 10: short chans              --1
	   * 12: long rate                --48000 (0xbb80)
	   * 16: long ave rate            --65655 (0x10077)
	   * 20: short align              --2
	   * 22: short data size (bits)   --16
	   * 24: bytes of extra
	   * ... some extra data dependent on format
	   *
	   *  R I  F F  # #  # #  W A  V E  f m  t sp
	   *  5249 4646 f851 0500 5741 5645 666d 7420
	   *  e40f 0000 0100 0100 80bb 0000 0077 0100
	   *  0200 1000 0000 0000 0000 0000 0000 0000
	   *  
	   *  #x000551f8 = 348664 = size in bytes - 8
	   *  #x00000fe4 = 4068 [fmt_ chunk size?]
	   */
	  got_fmt = true;
	  original_data_format = big_or_little_endian_short((unsigned char *)(hdrbuf + 8), little);
	  chans = big_or_little_endian_short((unsigned char *)(hdrbuf + 10), little);
	  srate = big_or_little_endian_int((unsigned char *)(hdrbuf + 12), little);
	  update_frames_location = 12 + offset;
	  block_align = big_or_little_endian_short((unsigned char *)(hdrbuf + 20), little);
	  bits_per_sample = big_or_little_endian_short((unsigned char *)(hdrbuf + 22), little);
	  if (original_data_format == -2) /* 0xFFFE = "extensible" : short size=22, short bits, long chanmap, short format */
	    original_data_format = big_or_little_endian_short((unsigned char *)(hdrbuf + 24 + 8), little);
	  data_format = wave_to_sndlib_format(original_data_format, bits_per_sample, little);
	}
      else
	{
	  if ((match_four_chars((unsigned char *)hdrbuf, I_data)) && (data_location == 0))
	    {
	      update_ssnd_location = offset + 4;
	      data_location = offset + 8;
	      data_size = big_or_little_endian_uint((unsigned char *)(hdrbuf + 4), little); /* was int 27-Jul-01 */
	      if (chunksize == 0) break; /* see aiff comment */
	    }
	  else
	    {
	      if (match_four_chars((unsigned char *)hdrbuf, I_fact))
		{
		  fact_samples = big_or_little_endian_int((unsigned char *)(hdrbuf + 8), little);
		}
	      else
		{
		  if (match_four_chars((unsigned char *)hdrbuf, I_inst))
		    {
		      base_note = hdrbuf[8];
		      base_detune = hdrbuf[9];
		      /* rest is gain low-note high-note low-velocity high-velocity */
		    }
		  else
		    {
		      if (match_four_chars((unsigned char *)hdrbuf, I_clm_))
			{
			  comment_start = offset + 8;
			  comment_end = comment_start + chunksize - 1; /* end of comment not start of next chunk */
			}
		      else
			{
			  if ((match_four_chars((unsigned char *)hdrbuf, I_LIST)) &&
			      (match_four_chars((unsigned char *)(hdrbuf + 8), I_INFO)))
			    {
			      aux_comment_start[0] = offset + 8;
			      aux_comment_end[0] = offset + 8 + chunksize - 1;
			    }
			}
		    }
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
    }
  if (!got_fmt)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no fmt chunk?", filename));
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no data chunk?", filename));
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

static int write_riff_header(int chan, int wsrate, int wchans, int siz, int format, const char *comment, int len)
{
  int i, j, lenhdr, extra;
  off_t offset, curend;
  lenhdr = 0;
  extra = 0;
  if (len != 0) 
    {
      lenhdr = 12;
      if ((len % 4) != 0)
	extra = (4 - (len % 4));
    }
  write_four_chars((unsigned char *)hdrbuf, I_RIFF);
  mus_lint_to_char((unsigned char *)(hdrbuf + 4), len + 36 + siz + lenhdr + extra);
  write_four_chars((unsigned char *)(hdrbuf + 8), I_WAVE);
  write_four_chars((unsigned char *)(hdrbuf + 12), I_fmt_);
  mus_lint_to_char((unsigned char *)(hdrbuf + 16), 24 - 8);
  switch (format)
    {
    case MUS_MULAW: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 7); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 8); 
      break;
    case MUS_ALAW: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 6); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 8); 
      break;
    case MUS_UBYTE: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 1); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 8); 
      break;
    case MUS_LSHORT: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 1); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 16); 
      break;
    case MUS_L24INT: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 1); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 24); 
      break;
    case MUS_LINT: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 1); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 32); 
      break;
    case MUS_LFLOAT: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 3); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 32); 
      break;
    case MUS_LDOUBLE: 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 20), 3); 
      mus_lshort_to_char((unsigned char *)(hdrbuf + 34), 64); 
      break;
    default: 
      return(mus_error(MUS_UNSUPPORTED_DATA_FORMAT, "riff header: can't write data format: %d (%s)",
		       format,
		       any_data_format_name(format)));
      break;
    }
  mus_lshort_to_char((unsigned char *)(hdrbuf + 22), (short)wchans);
  mus_lint_to_char((unsigned char *)(hdrbuf + 24), wsrate);
  mus_lint_to_char((unsigned char *)(hdrbuf + 28), wsrate * wchans * mus_bytes_per_sample(format)); /* added chans 10-Mar-99 */
  mus_lshort_to_char((unsigned char *)(hdrbuf + 32), (short)(wchans * mus_bytes_per_sample(format)));

  offset = 36;
  i = 36;
  curend = 0;
  if (len != 0)
    {
      offset += len + 12;
      write_four_chars((unsigned char *)(hdrbuf + 36), I_clm_);
      mus_lint_to_char((unsigned char *)(hdrbuf + 40), len + extra);
      i = 44;
      for (j = 0; j < len; j++)
	{
	  if (i == HDRBUFSIZ)
	    {
	      curend += HDRBUFSIZ;
	      CHK_WRITE(chan, hdrbuf, HDRBUFSIZ);
	      i = 0;
	    }
	  hdrbuf[i] = comment[j];
	  i++;
	}
      if (extra != 0)
	{
	  if ((i + extra) > HDRBUFSIZ)
	    {
	      curend += i;
	      CHK_WRITE(chan, hdrbuf, i);
	      i = 0;
	    }
	  for (j = 0; j < extra; j++)
	    {
	      hdrbuf[i] = 0;
	      i++;
	    }
	}
    }
  curend += i;
  CHK_WRITE(chan, hdrbuf, i);
  write_four_chars((unsigned char *)hdrbuf, I_data);
  mus_lint_to_char((unsigned char *)(hdrbuf + 4), siz);
  data_location = 8 + curend;
  CHK_WRITE(chan, hdrbuf, 8);
  return(MUS_NO_ERROR);
}

char *mus_header_riff_aux_comment(const char *name, off_t *starts, off_t *ends)
{
  char *sc = NULL, *auxcom;
  if ((starts) && (starts[0] != 0))
    {
      int len, j, fd, k, m;
      off_t i, end;
      /* found a LIST+INFO chunk (and no other comment) */
      fd = mus_file_open_read(name);
      if (fd == -1) return(NULL);
      i = starts[0];
      end = ends[0];
      sc = (char *)CALLOC(end - i + 2, sizeof(char));
      j = 0;
      k = 4;
      lseek(fd, i, SEEK_SET);
      auxcom = (char *)CALLOC(end - i + 2, sizeof(char));
      CHK_READ(fd, auxcom, end - i + 1);
      CLOSE(fd, name);
      i += 4;
      while (i < end)
	{
	  for (m = 0; m < 4; m++) sc[j++] = auxcom[k++];
	  len = mus_char_to_lint((unsigned char *)(auxcom + k));
	  if ((len <= 0) || (len > end)) break;
	  sc[j++] = ':';
	  sc[j++] = ' ';
	  k += 4;
	  for (m = 0; m < len; m++)
	    if (auxcom[k] != 0) 
	      sc[j++] = auxcom[k++]; 
	    else k++;
	  sc[j++] ='\n';
	  if (len & 1) 
	    {
	      len++; 
	      k++;
	    }
	  i += (len + 8);
	}
      FREE(auxcom);
    }
  return(sc);
}


/* soundforge -- just a quick hack until I get better documentation */
static long little_long_long(unsigned char *buf)
{
  return(mus_char_to_lint((unsigned char *)(buf)));
}

static int read_soundforge_header(const char *filename, int chan)
{
  /* like RIFF but lowercase and 64-bit vals */
  int chunksize, chunkloc, i, off;
  off_t offset;
  chunkloc = 12 * 2 + 16;
  offset = 0;
  data_format = MUS_UNKNOWN;
  srate = 0;
  chans = 0;
  fact_samples = 0;
  bits_per_sample = 0;
  for (i = 0; i < AUX_COMMENTS; i++) aux_comment_start[i] = 0;
  true_file_length = SEEK_FILE_LENGTH(chan);
  update_form_size = little_long_long((unsigned char *)(hdrbuf + 4 * 2));
  while (true)
    {
      offset += chunkloc;
      if (offset >= true_file_length) break;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 64) <= 0) break;
      chunksize = little_long_long((unsigned char *)(hdrbuf + 16));
      if ((chunksize == 0) && /* can be empty data chunk? */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if (match_four_chars((unsigned char *)hdrbuf, I_fmt_))
	{
	  off = 16;
	  original_data_format = mus_char_to_lshort((unsigned char *)(hdrbuf + 8 + off));
	  chans = mus_char_to_lshort((unsigned char *)(hdrbuf + 10 + off));
	  srate = mus_char_to_lint((unsigned char *)(hdrbuf + 12 + off));
	  block_align = mus_char_to_lshort((unsigned char *)(hdrbuf + 20 + off));
	  bits_per_sample = mus_char_to_lshort((unsigned char *)(hdrbuf + 22 + off));
	  data_format = wave_to_sndlib_format(original_data_format, bits_per_sample, true);
	}
      else
	{
	  if ((match_four_chars((unsigned char *)hdrbuf, I_data)) && (data_location == 0))
	    {
	      data_location = offset + 16 + 8;
	      data_size = mus_char_to_ulint((unsigned char *)(hdrbuf + 16));
	      if (chunksize == 0) break; /* see aiff comment */
	    }
	  else
	    {
	      if (match_four_chars((unsigned char *)hdrbuf, I_fact))
		{
		  fact_samples = mus_char_to_lint((unsigned char *)(hdrbuf + 8));
		}
	    }
	}
      chunkloc = (8 + chunksize);
      chunkloc -= 8; 
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no data chunk?", filename));
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ AVI ------------------------------------
 * actually a video format, but it sometimes contains embedded 'wave' data
 *
 * RIFF xxx AVI 
 *   <various LISTs>
 *   LIST xxxx hdr1 LIST strl(?) strh | strf | strn etc
 *     strf is the WAVE header starting with the sound format
 *   LIST xxxx movi ##db|##wb -- wb subblocks have the audio data (these need to be collected as a single stream)
 * there are many complications that we make no effort to handle here
 *
 * described in http://www.rahul.net/jfm/avi.html
 */

static int read_avi_header(const char *filename, int chan)
{
  /* we know we have checked for RIFF xxxx AVI  when we arrive here */
  int chunksize, chunkloc, cksize, bits;
  bool happy;
  off_t ckoff, cktotal, offset;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 8));
  chunkloc = 12;
  offset = 0;
  data_format = MUS_UNKNOWN;
  srate = 0;
  chans = 1;
  happy = true;
  true_file_length = SEEK_FILE_LENGTH(chan);
  while (happy)
    {
      offset += chunkloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	return(mus_error(MUS_HEADER_READ_FAILED, "%s avi header: chunks confused at " OFF_TD, filename, offset));
      chunksize = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
      if ((chunksize == 0) && /* can be empty data chunk? */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if (match_four_chars((unsigned char *)hdrbuf, I_LIST))
	{
	  ckoff = offset + 12;
	  cktotal = 12;
	  if (match_four_chars((unsigned char *)(hdrbuf + 8), I_movi))
	    {
	      while (cktotal < chunksize)
		{
		  lseek(chan, ckoff, SEEK_SET);
		  CHK_READ(chan, hdrbuf, 8);
		  cksize = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
		  if ((hdrbuf[2] == 'w') && (hdrbuf[3] == 'b'))
		    {
		      data_location = ckoff;
		      if (srate != 0) happy = false;
		      break;
		    }
		  ckoff += (8 + cksize);
		  cktotal += (8 + cksize);
		}
	    }
	  else
	    {
	      while (cktotal < chunksize)
		{
		  lseek(chan, ckoff, SEEK_SET);
		  CHK_READ(chan, hdrbuf, 8);
		  cksize = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
		  ckoff += (8 + cksize);
		  cktotal += (8 + cksize);
		  if (match_four_chars((unsigned char *)hdrbuf, I_LIST))
		    {
		      off_t cksizer, ckoffr, cktotalr, rdsize;
		      ckoffr = ckoff + 12;
		      cktotalr = 12;
		      while (cktotalr < cksize)
			{
			  lseek(chan, ckoffr, SEEK_SET);
			  CHK_READ(chan, hdrbuf, 8);
			  cksizer = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
			  ckoffr += (8 + cksizer);
			  cktotalr += (8 + cksizer);
			  if (match_four_chars((unsigned char *)hdrbuf, I_strf))
			    {
			      if (cksizer < HDRBUFSIZ) 
				rdsize = cksizer; 
			      else rdsize = HDRBUFSIZ;
			      CHK_READ(chan, hdrbuf, rdsize);
			      original_data_format = mus_char_to_lshort((unsigned char *)hdrbuf);
			      chans = mus_char_to_lshort((unsigned char *)(hdrbuf + 2));
			      srate = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
			      /* block_align = mus_char_to_lshort((unsigned char *)(hdrbuf + 12)); */
			      bits = mus_char_to_lshort((unsigned char *)(hdrbuf + 14));
			      /* only 16 bit linear little endian for now */
			      if ((bits == 16) && (original_data_format == 1))
				data_format = MUS_LSHORT;
			      if (data_location != 0) happy = false;
			      break;
			    }
			}
		    }
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no movi chunk?", filename));
  if (data_location > true_file_length) 
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ SoundFont 2.0 ------------------------------------
 *
 * Emu's SoundFont(tm) format uses RIFF -- at ftp.creaf.com:/pub/emu/sf2_00a.ps)
 *
 * RIFF xxxx sfbk followed by
 *   LIST xxxx INFO chunk (nothing of interest -- icmt subchunk might have comments)
 *   LIST xxxx sdta chunk = data
 *     smpl chunk (16 bit linear little-endian)
 *   LIST xxxx pdta list chunk 
 *     shdr subchunk has srate at 40 (int), samples at 28
 *
 * http://smurf.sourceforge.net/sfont_intro.php
 * http://www.hammersound.net/
 */

static int soundfont_entries = 0;
static int *soundfont_starts = NULL, *soundfont_ends = NULL, *soundfont_loop_starts = NULL, *soundfont_loop_ends = NULL;
static int soundfont_size = 0;
static char **soundfont_names = NULL;

static void soundfont_entry(const char *name, int start, int end, int loop_start, int loop_end)
{
  if (soundfont_entries == soundfont_size)
    {
      int i;
      if (soundfont_size == 0)
	{
	  soundfont_size = 8;
	  soundfont_starts = (int *)CALLOC(soundfont_size, sizeof(int));
	  soundfont_ends = (int *)CALLOC(soundfont_size, sizeof(int));
	  soundfont_loop_starts = (int *)CALLOC(soundfont_size, sizeof(int));
	  soundfont_loop_ends = (int *)CALLOC(soundfont_size, sizeof(int));
	  soundfont_names = (char **)CALLOC(soundfont_size, sizeof(char *));
	}
      else
	{
	  soundfont_size += 8;
	  soundfont_starts = (int *)REALLOC(soundfont_starts, soundfont_size * sizeof(int));
	  soundfont_ends = (int *)REALLOC(soundfont_ends, soundfont_size * sizeof(int));
	  soundfont_loop_starts = (int *)REALLOC(soundfont_loop_starts, soundfont_size * sizeof(int));
	  soundfont_loop_ends = (int *)REALLOC(soundfont_loop_ends, soundfont_size * sizeof(int));
	  soundfont_names = (char **)REALLOC(soundfont_names, soundfont_size * sizeof(char *));
	}
      for (i = soundfont_entries; i < soundfont_size; i++) soundfont_names[i] = NULL;
    }
  if (soundfont_names[soundfont_entries] == NULL) soundfont_names[soundfont_entries] = (char *)CALLOC(20, sizeof(char));
  strcpy(soundfont_names[soundfont_entries], name);
  soundfont_starts[soundfont_entries] = start;
  soundfont_ends[soundfont_entries] = end;
  soundfont_loop_starts[soundfont_entries] = loop_start;
  soundfont_loop_ends[soundfont_entries] = loop_end;
  soundfont_entries++;
}

int mus_header_sf2_entries(void) {return(soundfont_entries);}
char *mus_header_sf2_name(int n) {return(soundfont_names[n]);}
int mus_header_sf2_start(int n) {return(soundfont_starts[n]);}
int mus_header_sf2_end(int n) {return(soundfont_ends[n]);}
int mus_header_sf2_loop_start(int n) {return(soundfont_loop_starts[n]);}
int mus_header_sf2_loop_end(int n) {return(soundfont_loop_ends[n]);}

static int read_soundfont_header(const char *filename, int chan)
{
  /* we know we have checked for RIFF xxxx sfbk when we arrive here */
  int chunksize, chunkloc, type, cksize, i, this_end, last_end;
  off_t ckoff, offset;
  bool happy;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 8));
  chunkloc = 12;
  offset = 0;
  soundfont_entries = 0;
  data_format = MUS_LSHORT;
  srate = 0;
  chans = 1; 
  happy = true;
  last_end = 0;
  true_file_length = SEEK_FILE_LENGTH(chan);
  while (happy)
    {
      offset += chunkloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	return(mus_error(MUS_HEADER_READ_FAILED, "%s soundfont header: chunks confused at " OFF_TD, filename, offset));
      chunksize = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
      if ((chunksize == 0) && /* can be empty data chunk? */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if (match_four_chars((unsigned char *)hdrbuf, I_LIST))
	{
	  /* everything is squirreled away in LIST chunks in this format */
	  if (match_four_chars((unsigned char *)(hdrbuf + 8), I_pdta))
	    {
	      /* go searching for I_shdr -- headers this complicated should be illegal. */
	      ckoff = offset + 12;
	      lseek(chan, ckoff, SEEK_SET);
	      while (srate == 0)
		{
		  size_t bytes;
		  bytes = read(chan, hdrbuf, 8);
		  if (bytes == 0)
		    {
		      happy = false;
		      break;
		    }
		  i = 0;
		  cksize = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
		  ckoff += (8 + cksize);
		  /* here we need to jump over subchunks! -- 4-Aug-97 */
		  if (match_four_chars((unsigned char *)hdrbuf, I_shdr))
		    {
		      /* each sound: 
		       *  0: name
		       * 20: start (in samples from start of bank)
		       * 24: end
		       * 28: loop start (also relative to start of bank)
		       * 32: loop end
		       * 36: sample rate
		       * 40: pitch (60 = middle C)
		       * 41: detune (cents)
		       * 42: link (to other channel if any?)
		       * 44: type (1 = mono, 2 = mono right, 4 = mono left, others (0x8000) apparently for ROM presets?)
		       */
		      while (i < cksize)
			{
			  CHK_READ(chan, hdrbuf, 46);
			  i += 46;
			  type = mus_char_to_lshort((unsigned char *)(hdrbuf + 44));
			  if ((type == 1) &&
			      (mus_char_to_lint((unsigned char *)(hdrbuf + 24)) > 0)) 
			    {
			      if (srate == 0) 
				srate = mus_char_to_lint((unsigned char *)(hdrbuf + 36));
			      soundfont_entry((char *)(hdrbuf),
					      mus_char_to_lint((unsigned char *)(hdrbuf + 20)),
					      this_end = mus_char_to_lint((unsigned char *)(hdrbuf + 24)),
					      mus_char_to_lint((unsigned char *)(hdrbuf + 28)),
					      mus_char_to_lint((unsigned char *)(hdrbuf + 32)));
			      if (this_end > last_end) last_end = this_end;
			    }
			}
		      happy = (data_location == 0);
		    }
		  else 
		    {
		      if (ckoff >= offset + 8 + chunksize) 
			break;
		      lseek(chan, ckoff, SEEK_SET);
		    }
		}
	    }
	  else
	    {
	      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_sdta))
		{
		  /* assume smpl follows + subchunk size */
		  /* Convert 1.4 appears to create a separate smpl chunk */
		  data_location = offset + 20; /* LIST xxxx sdta smpl xxxx ... */
		  happy = (srate == 0);
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
    }
  if (srate == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: srate == 0", filename));
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no sdta chunk?", filename));
  if (last_end > 0)
    data_size = last_end; /* samples already */
  else data_size = (true_file_length - data_location) / 2;
  return(MUS_NO_ERROR);
}




/* ------------------------------------ NIST ------------------------------------ 
 *
 * code available in ogitools-v1.0.tar.gz at svr-ftp.eng.cam.ac.uk:comp.speech/sources
 * 
 *   0: "NIST_1A"
 *   8: data_location as ASCII representation of integer (apparently always "   1024")
 *  16: start of complicated header -- see below for details
 *
 *  The most recent version of the SPHERE package is available
 *  via anonymous ftp from jaguar.ncsl.nist.gov [129.6.48.157] in the pub directory
 *  in compressed tar form as "sphere-v.tar.Z" (where "v" is the version
 *  code 2.6a last I looked).  shortpack is also at this site.
 *
 *  here's an example:
 *
 *  NIST_1A
 *     1024
 *  database_id -s5 TIMIT
 *  database_version -s3 1.0
 *  utterance_id -s8 aks0_sa1
 *  channel_count -i 1
 *  sample_count -i 63488
 *  sample_rate -i 16000
 *  sample_min -i -6967
 *  sample_max -i 7710
 *  sample_n_bytes -i 2
 *  sample_byte_format -s2 01
 *  sample_sig_bits -i 16
 *  end_head
 *
 * the sample_byte_format can be "10"=big-endian or "01"=little-endian, or "shortpack-v0"=compressed via shortpack
 * other formats are wavpack and shorten.
 *
 * another field is 'sample_coding' which can be pcm (i.e. linear), 'pcm, embedded-shorten-v1.09', mu-law, alaw, ulaw, pculaw etc --
 *   so unpredictable as to be totally useless. This means we sometimes try to decode shorten-encoded files because
 *   we ignore this field.  And worse, there's a 'channels_interleaved' field that (apparently) can be false.  Tough.
 */

#define MAX_FIELD_LENGTH 80

static int decode_nist_value(char *str, int base, int end)
{
  /* can be -i -r or -snnn where nnn = ascii rep of integer = len of string (!) */
  /* we'll deal only with integer fields (and well-behaved string fields) */
  int i, j;
  char value[MAX_FIELD_LENGTH];
  memset((void *)value, 0, MAX_FIELD_LENGTH);
  i = base;
  while ((i < end) && (i < MAX_FIELD_LENGTH) && (str[i] != '-')) i++; /* look for -i or whatever */
  while ((i < end) && (i < MAX_FIELD_LENGTH) && (str[i] != ' ')) i++; /* look for space after it */
  i++;
  if (i >= MAX_FIELD_LENGTH) return(0);
  for (j = 0; i < end; j++, i++)
    value[j] = str[i];
  value[j] = 0;
  if (value[0] =='s') return(MUS_NIST_SHORTPACK);
  sscanf(value, "%d", &i);
  return(i);
}

static int read_nist_header(const char *filename, int chan)
{
  char str[MAX_FIELD_LENGTH], name[MAX_FIELD_LENGTH];
  bool happy = true;
  off_t curbase;
  int k, hend, j, n, nm, samples, bytes, byte_format, idata_location = 0;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf); /* the actual id is "NIST_1A" */
  memset((void *)str, 0, MAX_FIELD_LENGTH);
  memset((void *)name, 0, MAX_FIELD_LENGTH);
  for (k = 8; k < 16; k++) 
    str[k - 8] = hdrbuf[k];
  sscanf(str, "%d", &idata_location);       /* always "1024" */
  if (idata_location != 1024)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s NIST data location: %d?", filename, idata_location));
  data_location = 1024;
  n = 16;
  hend = INITIAL_READ_SIZE;
  k = 0;
  curbase = 0;
  samples = 0;
  bytes = 0;
  srate = 0;
  chans = 0;
  comment_start = 16;
  comment_end = 16;
  byte_format = 10;
  for (j = 0; j < MAX_FIELD_LENGTH; j++) 
    str[j] =' ';  
  while (happy) 
    {
      /* much as in xIFF files, march through the file looking for the data we're after */
      /* in this case we munch a character at a time... */
      str[k] = hdrbuf[n];
      if ((((str[k] == '\0') || (str[k] == '\n')) || ((curbase + n + 1) >= data_location)) || (k == 79))
	{
	  /* got a complete record (assuming no embedded newlines, of course) */
	  /* now look for a record we care about and decode it */
	  nm = 0;
	  while ((nm < MAX_FIELD_LENGTH) && (str[nm] != ' ') && (str[nm] != '\0') && (str[nm] != '\n'))
	    {
	      name[nm] = str[nm];
	      nm++;
	    }
	  if (nm >= MAX_FIELD_LENGTH) 
	    {
	      header_type = MUS_RAW; 
	      data_format = MUS_UNKNOWN; 
	      return(mus_error(MUS_UNSUPPORTED_HEADER_TYPE, "%s nist header: unreadable field (length = %d)?", filename, nm));
	    }
	  name[nm] = 0;
	  if (strcmp(name, "sample_rate") == 0) srate = decode_nist_value(str, nm, k); else
	    if (strcmp(name, "channel_count") == 0) chans = decode_nist_value(str, nm, k); else
	      if (strcmp(name, "end_head") == 0) {happy = false; comment_end = curbase + n - 9;} else
		if (strcmp(name, "sample_count") == 0) samples = decode_nist_value(str, nm, k); else
		  if ((bytes == 0) && (strcmp(name, "sample_n_bytes") == 0)) bytes = decode_nist_value(str, nm, k); else
		    if ((bytes == 0) && (strcmp(name, "sample_sig_bits") == 0)) {bytes = decode_nist_value(str, nm, k); bytes = (bytes >> 3);} else
		      if (strcmp(name, "sample_byte_format") == 0) byte_format = decode_nist_value(str, nm, k);
	  for (j = 0; j <= k; j++) str[j] =' ';
	  k = 0;
	  if ((curbase + n + 1) > 1024) happy = false;
	}
      else
	k++;
      n++;
      if (n >= hend)
	{
	  int read_bytes;
	  curbase += hend;
	  n = 0;
	  read_bytes = read(chan, hdrbuf, HDRBUFSIZ);
	  if (read_bytes < HDRBUFSIZ)
	    return(mus_error(MUS_HEADER_READ_FAILED, "%s NIST header truncated?", filename));
	  hend = HDRBUFSIZ;
	}
    }
  data_size = samples * bytes;
  if (byte_format == MUS_NIST_SHORTPACK)
    {
      data_format = MUS_UNKNOWN;
      original_data_format = MUS_NIST_SHORTPACK;
    }
  else
    {
      switch (bytes)
	{
	case 1: data_format = MUS_MULAW; break;
	case 2:
	  if (byte_format == 10) 
	    data_format = MUS_BSHORT;
	  else data_format = MUS_LSHORT;
	  break;
	case 3:
	  if (byte_format == 10) 
	    data_format = MUS_B24INT;
	  else data_format = MUS_L24INT;
	  break;
	case 4:
	  if (byte_format == 10) 
	    data_format = MUS_BINT;
	  else data_format = MUS_LINT;
	  break;
	default: data_format = MUS_BYTE; break;
	}
    }
  true_file_length = SEEK_FILE_LENGTH(chan);
  if ((data_size > true_file_length) && (original_data_format != MUS_NIST_SHORTPACK))
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

static int write_nist_header(int chan, int wsrate, int wchans, int siz, int format)
{
  char *header;
  int datum;
  datum = mus_bytes_per_sample(format);
  header = (char *)CALLOC(1024, sizeof(char));
  sprintf(header, "NIST_1A\n   1024\nchannel_count -i %d\nsample_rate -i %d\nsample_n_bytes -i %d\nsample_byte_format -s2 %s\nsample_sig_bits -i %d\nsample_count -i %d\nend_head\n",
	  wchans, wsrate, datum,
	  ((format == MUS_BSHORT) || (format == MUS_B24INT) || (format == MUS_BINT)) ? "10" : "01",
	  datum * 8, siz / datum);
  CHK_WRITE(chan, (unsigned char *)header, 1024);
  data_location = 1024;
  FREE(header);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ BICSF ------------------------------------ 
 * (actually, this is EBICSF and the old BICSF is called IRCAM below)
 *
 * 0-28: NeXT-compatible header, read by read_next_header above.
 *   28: bicsf magic number (107364 or trouble)
 *   32: srate as a 32-bit float
 *   36: chans
 *   40: data format indicator (2 = 16-bit linear, 4 = 32-bit float)
 *   44: begin chunks, if any
 *
 * followed by AIFF-style chunked header info with chunks like:
 *
 *   COMM size comment
 *   MAXA size {max amps (up to 4)} (frame offsets) time-tag unix msec counter
 *   CUE, PRNT, ENV etc 
 *
 * except in Paul Lansky's "hybrid" headers, according to MixViews.
 */

static int read_bicsf_header(const char *filename, int chan)
{
  int chunksize, chunkname, offset, chunkloc;
  bool happy;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 28));
  header_type = MUS_BICSF;
  data_location = 1024;
  if (data_size == 0) data_size = (true_file_length - data_location);
  lseek(chan, 40, SEEK_SET);
  CHK_READ(chan, hdrbuf, HDRBUFSIZ);
  original_data_format = mus_char_to_bint((unsigned char *)hdrbuf);
  switch (original_data_format) 
    {
    case 2: data_format = MUS_BSHORT; break;
    case 4: data_format = MUS_BFLOAT; break;
    case 8: data_format = MUS_BDOUBLE; break;
    default: break;
    }

  /* now check for a COMM chunk, setting the comment pointers */
  chunkloc = 4; /* next header + magic number, srate, chans, packing, then chunks, I think */
  offset = 40;
  happy = true;
  while (happy)
    {
      if (((offset + chunkloc) >= data_location) ||
	  ((offset + chunkloc) < 40))
	happy = false;
      else
	{
	  offset += chunkloc;
	  if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	    return(mus_error(MUS_HEADER_READ_FAILED, "%s bicsf header: chunks confused at %d", filename, offset));
	  chunkname = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
	  chunksize = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
	  if (match_four_chars((unsigned char *)hdrbuf, I_COMM))
	    {
	      comment_start = 8 + offset;
	      comment_end = comment_start + chunksize -1;
	      happy = false;
	    }
	  else
	    {
	      if ((chunkname == 0) || (chunksize <= 0)) 
		happy = false;
	    }
	  chunkloc = (8 + chunksize);
	}
    }
  return(MUS_NO_ERROR);
  /* from here we fall back into read_next_header */
}



/* ------------------------------------ IRCAM ------------------------------------ 
 * read/write CLM (old-style BICSF) -- added write option for Sun port 12-Dec-94
 *
 *    0: 0x1a364 or variations thereof -- byte order gives big/little_endian decision,
 *         ^ digit gives machine info, according to AFsp sources -- see IRCAM ints above
 *    4: srate as a 32-bit float
 *    8: chans
 *   12: data format indicator (2 = 16-bit linear, 4 = 32-bit float)
 *       according to new Sox (version 11), these packing modes are now bytes/sample in low short, code in high
 *       so 1 = char, 0x10001 = alaw, 0x20001 = mulaw, 2 = short, 3 = 24bit?, 0x40004 = long, 4 = float (AFsp sez 4 can also be double)
 *   16: comment start -- how to tell if it's a real comment?
 *       apparently these are separated as short code, short blocksize, then data
 *       codes: 0 = end, 1 = maxamp, 2 = comment, 3 = pvdata, 4 = audioencode and codemax??
 * 1024: data start
 * 
 * apparently the byte order depends on the machine.
 * and yet... convert 1.4 makes a .sf file with little endian header, the VAX id, and big endian data?
 *            Csound also uses the VAX magic number with little-endian unscaled floats!  Argh. 
 *            even worse, Paul Lansky plops some version of this at the end of a NeXT header!  Complete chaos...
 */

static int read_ircam_header(const char *filename, int chan)
{
  short bcode, bloc, bsize;
  int offset;
  bool little, happy;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
  if ((mus_char_to_lint((unsigned char *)hdrbuf) == I_IRCAM_VAX) || 
      (mus_char_to_lint((unsigned char *)hdrbuf) == I_IRCAM_MIPS))
    little = true;
  else little = false;
  little_endian = little;
  data_location = 1024;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - 1024);
  original_data_format = big_or_little_endian_int((unsigned char *)(hdrbuf + 12), little);
  data_format = MUS_UNKNOWN;
  if (original_data_format == 2) 
    {
      if (little) 
	data_format = MUS_LSHORT; 
      else data_format = MUS_BSHORT;
    }
  else if (original_data_format == 4) 
    {
      if (little) 
	{
	  if (mus_char_to_lint((unsigned char *)hdrbuf) == I_IRCAM_VAX)
	    data_format = MUS_LFLOAT_UNSCALED; /* Csound and MixViews */
	  else data_format = MUS_LFLOAT;
	}
      else data_format = MUS_BFLOAT;
    }
  else if (original_data_format == 0x40004) 
    {
      if (little) data_format = MUS_LINT;
      else data_format = MUS_BINT;
    }
  else if (original_data_format == 0x10001) data_format = MUS_ALAW;
  else if (original_data_format == 0x20001) data_format = MUS_MULAW;
  else if (original_data_format == 1) data_format = MUS_BYTE;
  else if (original_data_format == 3)
    {
      if (little) data_format = MUS_L24INT;
      else data_format = MUS_B24INT;
    }
  else if (original_data_format == 8)
    {
      if (little) data_format = MUS_LDOUBLE;
      else data_format = MUS_BDOUBLE;
    }
  srate = (int)big_or_little_endian_float((unsigned char *)(hdrbuf + 4), little);
  chans = big_or_little_endian_int((unsigned char *)(hdrbuf + 8), little);
  bloc = 16;
  happy = true;
  offset = 0;
  while (happy)
    {
      offset += bloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	return(mus_error(MUS_HEADER_READ_FAILED, "%s ircam header: chunks confused at %d", filename, offset));
      bcode = big_or_little_endian_short((unsigned char *)hdrbuf, little);
      bsize = big_or_little_endian_short((unsigned char *)(hdrbuf + 2), little);
      if (bcode == 2)
	{
	  happy = false;
	  comment_start = 4 + offset;
	  comment_end = comment_start + bsize - 1; /* was -5? */
	}
      bloc = bsize;
      if ((bsize <= 0) || (bcode <= 0) || ((offset + bloc) > 1023)) happy = false;
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

static int sndlib_format_to_ircam(int format)
{
  switch (format)
    {
    case MUS_MULAW:  return(0x20001); break;
    case MUS_ALAW:   return(0x10001); break;
    case MUS_BSHORT: return(2);       break;
    case MUS_BINT:   return(0x40004); break;
    case MUS_BFLOAT: return(4);       break;
    default: 
      return(mus_error(MUS_UNSUPPORTED_DATA_FORMAT, "IRCAM header unsupported data format: %d (%s)", format, any_data_format_name(format)));
      break;
    }
}

static void write_ircam_comment(int fd, const char *comment, int len)
{
  if (len > 0)
    {
      mus_bshort_to_char((unsigned char *)hdrbuf, 2);
      mus_bshort_to_char((unsigned char *)(hdrbuf + 2), (short)len);
      CHK_WRITE(fd, hdrbuf, 4);
      CHK_WRITE(fd, (unsigned char *)comment, len);
    }
  else
    {
      mus_bint_to_char((unsigned char *)hdrbuf, 0);
      CHK_WRITE(fd, hdrbuf, 4);
    }
  len = 1024 - (len + 20);
  if (len > 0)
    {
      unsigned char *combuf;
      combuf = (unsigned char *)CALLOC(len, sizeof(char));
      CHK_WRITE(fd, combuf, len);
      FREE(combuf);
    }
}

static int write_ircam_header(int chan, int wsrate, int wchans, int format, const char *comment, int len)
{
  mus_bint_to_char((unsigned char *)hdrbuf, 0x2a364); /* SUN id */
  mus_bfloat_to_char((unsigned char *)(hdrbuf + 4), (float)wsrate);
  mus_bint_to_char((unsigned char *)(hdrbuf + 8), wchans);
  mus_bint_to_char((unsigned char *)(hdrbuf + 12), sndlib_format_to_ircam(format));
  CHK_WRITE(chan, hdrbuf, 16);
  data_location = 1024;
  write_ircam_comment(chan, comment, len);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ 8SVX ------------------------------------- 
 * (also known as IFF)
 *
 * very similar to AIFF:
 *  "BODY" => [4] samples [n] data
 *  "VHDR" => srate (short)
 *  "CHAN" => chans
 *  "ANNO" and "NAME"
 *
 * big_endian throughout
 */

static int read_8svx_header(const char *filename, int chan, bool bytewise)
{
  int chunksize, offset, chunkloc;
  bool happy;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
  chunkloc = 12;
  offset = 0;
  if (bytewise) data_format = MUS_BYTE; else data_format = MUS_BSHORT;
  srate = 0;
  chans = 1;
  happy = true;
  true_file_length = SEEK_FILE_LENGTH(chan);
  update_form_size = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
  while (happy)
    {
      offset += chunkloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	return(mus_error(MUS_HEADER_READ_FAILED, "%s 8svx header: chunks confused at %d", filename, offset));
      chunksize = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
      if ((chunksize == 0) && /* can be empty data chunk? */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if (match_four_chars((unsigned char *)hdrbuf, I_CHAN))
	{
	  chans = mus_char_to_bint((unsigned char *)(hdrbuf + 8));
	  chans = (chans & 0x01) + 
	          ((chans & 0x02) >> 1) + 
	          ((chans & 0x04) >> 2) + 
                  ((chans & 0x08) >> 3);
	  /* what in heaven's name is this?  Each bit corresponds to a channel? */
	}
      else
	{
	  if (match_four_chars((unsigned char *)hdrbuf, I_VHDR))
	    {
	      /* num_samples (int) at hdrbuf + 8 */
	      srate = mus_char_to_ubshort((unsigned char *)(hdrbuf + 20));
	      original_data_format = hdrbuf[23];
	      if (original_data_format != 0) 
		data_format = MUS_UNKNOWN;
	    }
	  else
	    {
	      if ((match_four_chars((unsigned char *)hdrbuf, I_ANNO)) || 
		  (match_four_chars((unsigned char *)hdrbuf, I_NAME)))
		{
		  comment_start = offset + 8;
		  comment_end = comment_start + chunksize - 1;
		}
	      else
		{
		  if (match_four_chars((unsigned char *)hdrbuf, I_BODY))
		    {
		      data_size = chunksize;
		      data_location = offset + 12;
		      happy = false;
		    }
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no BODY chunk?", filename));
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ VOC -------------------------------------- 
 *
 *   0: "Creative Voice File" followed by a couple ctrl-Z ('32) (swapped data)
 *  20: header end (short) {8svx, 26 = data_offset, 0x10a = version, ((~version + 0x1234) & 0xffff) = 0x1129}
 * [20]: first block:
 *     block code, 1 = data, 0 = end, 9 = data_16 (2 = continue, 3 = silence, 4 = marker, 5 = text, 6 = loop, 7 = loop-end, 8 = extended)
 *     block len as 24 bit int(?)
 *     if data, then rate code (byte), then data (assuming 8-bit unsigned, mono)
 *     if data_16, long srate, byte: data size (8 or 16), byte chans
 *     if text, ascii text (a comment)
 *     if extended (8) precedes 1 (data): 8 4 then time constant (short), byte: packing code (0), byte chans (0 = mono)
 *
 * apparently always little_endian
 * updated extensively 29-Aug-95 from sox10 voc.c
 */

static int read_voc_header(const char *filename, int chan)
{
  off_t curbase;
  int type, len, voc_extended, bits, code;
  bool happy;
  data_format = MUS_UBYTE;
  chans = 1;
  happy = true;
  voc_extended = 0;
  true_file_length = SEEK_FILE_LENGTH(chan);
  curbase = mus_char_to_lshort((unsigned char *)(hdrbuf + 20));
  if (true_file_length < curbase)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: block location " OFF_TD " > file length: " OFF_TD, filename, curbase, true_file_length));
  lseek(chan, curbase, SEEK_SET);
  CHK_READ(chan, hdrbuf, HDRBUFSIZ);
  while (happy)
    {
      type = (int)(hdrbuf[0]);
      len = (((int)hdrbuf[3]) << 16) + (((int)hdrbuf[2]) << 8) + (((int)hdrbuf[1]));
      if (type == 1) /* voc_data */
	{
	  data_size = len - 1; /* was -3 */
	  data_location = curbase + 6;
	  if (voc_extended == 0) 
	    {
	      srate = (int)(1000000.0 / (256 - ((int)(hdrbuf[4] & 0xff))));
	      original_data_format = hdrbuf[5];
	      if (hdrbuf[5] == 0) 
		data_format = MUS_UBYTE; 
	      else data_format = MUS_UNKNOWN;
	    }
	  happy = false;
	}
      else
	{
	  if (type == 9) /* voc_data_16 */
	    {
	      data_size = len - 1; /* was -3 */
	      data_location = curbase + 6;
	      srate = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
	      bits = ((int)hdrbuf[8]);
	      if (bits == 8)
		{
		  code = mus_char_to_lshort((unsigned char *)(hdrbuf + 10));
		  if (code == 6) 
		    data_format = MUS_ALAW;
		  else
		    if (code == 7)
		      data_format = MUS_MULAW;
		    else data_format = MUS_UBYTE; 
		}
	      else 
		if (bits == 16) 
		  data_format = MUS_LSHORT;
		else data_format = MUS_UNKNOWN;
	      chans = (int)hdrbuf[9];
	      if (chans == 0) chans = 1;
	      happy = false;
	    }
	  else
	    {
	      if (((len + curbase) < true_file_length) && (type != 0))
		{
		  if (type == 5) /* voc_text */
		    {
		      comment_start = curbase + 4;
		      comment_end = comment_start + len - 1;
		    }
		  else
		    {
		      if (type == 8) /* voc_extended */
			{
			  srate = (256000000 / (65536 - mus_char_to_lshort((unsigned char *)(hdrbuf + 4))));
			  if ((int)(hdrbuf[7]) == 0) chans = 1; else chans = 2;
			  if ((int)(hdrbuf[6]) != 0) data_format = MUS_UNKNOWN;
			}
		      /* I'd add loop support here if I had any example sound files to test with */
		    }
		  if (seek_and_read(chan, (unsigned char *)hdrbuf, curbase + len + 4, HDRBUFSIZ) <= 0)
		    return(mus_error(MUS_HEADER_READ_FAILED, "%s voc header: ran off end of file", filename));
		  curbase += len;
		}
	      else happy = false;
	    }
	}
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no data(type 1 or 9) chunk?", filename));
  if ((data_size > true_file_length) || (data_size < (off_t)(true_file_length / 10))) /* some VOC files seem to have completely bogus lengths */
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ TwinVQ ------------------------------------ 
 *
 * from Audio Tools Library (atl.zip) at http://jfaul.de/atl.
 * a chunked header for proprietary (and apparently obsolete?) compressed data
 *
 * 0: "TWIN"
 * 4: version id (string)
 * 12: header size ["cardinal" -> bint]
 * common chunk header (4 of ID, bint size)
 * 24: channels (bint: 0=mono 1=stereo)
 * bitrate (bint)
 * 32: srate (bint khz 11, 22, 44 else *1000)
 * security (bint 0)
 * filesize (bint bytes)
 * possible chunks: NAME COMT AUTH (c) FILE ALBM DATA
 */ 

/* Monkey files start with "MAC ", but this is yet another compression-oriented format, I think (APE?) */

static int read_twinvq_header(const char *filename, int chan)
{
  data_format = MUS_UNKNOWN;
  data_location = mus_char_to_bint((unsigned char *)(hdrbuf + 12)) + 16 + 8;
  chans = 1 + mus_char_to_bint((unsigned char *)(hdrbuf + 24));
  srate = mus_char_to_bint((unsigned char *)(hdrbuf + 32));
  if (srate == 11) srate = 11025; else
    if (srate == 22) srate = 22050; else
      if (srate == 44) srate = 44100; else
	srate *= 1000;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  return(MUS_NO_ERROR);
}


static int read_sdif_header(const char *filename, int chan)
{
  /* yeah, right! */
  return(MUS_UNSUPPORTED);
}

static int read_nvf_header(const char *filename, int chan)
{
  /* info from nvftools by Tom Mander: */
  /*
    Numbers stored little-endian.

    bytes 0-3:   "NVF "                 magic number
    bytes 4-7:    0x00000001            NVF version number?
    bytes 8-11:   0x00000020            size of rest of header
    bytes 12-15:  "VFMT"                VFMT chunk h
    bytes 16-19:  0x00000001            VFMT version number?
    bytes 20-23:  0x00000014            size of reset of VFMT header
    bytes 24-27:  0x00007D00            32000 bit/s bitrate
    bytes 28-29:  0x0001                channels
    bytes 30-31:  0x0000                unknown
    bytes 32-35:  0x00001F40            8000kHz sample rate
    bytes 36-39:  0x00003E80            16000
    bytes 40-41:  0x0002                width in bytes of uncompressed data?
    bytes 42-43:  0x0010                width in bits of compressed data?

    The rest of the data is G.721 data nibble packing big-endian, 4bits per
    sample (nibble) single channel at 32kbit. When the Nomad records an NVF
    file it does it in 92 sample (46 byte) frames or 0.0115sec.
  */
  if (mus_char_to_lint((unsigned char *)(hdrbuf + 4)) != 1) return(mus_error(MUS_HEADER_READ_FAILED, "%s: NVF[4] != 1", filename));
  if (!(match_four_chars((unsigned char *)(hdrbuf + 12), I_VFMT))) return(mus_error(MUS_HEADER_READ_FAILED, "%s: no VFMT chunk", filename));
  data_format = MUS_UNKNOWN; /* g721 --translate elsewhere */
  chans = 1;
  srate = 8000;
  data_location = 44;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location) * 2; /* 4 bit samps? */
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  return(MUS_NO_ERROR);
}


/* ------------------------------------ ADC ------------------------------------ 
 * also known as OGI format
 * TIMIT format is identical except it omits the data format field (header size claims to be bytes)
 *
 * from ad.h and other files, ogitools-v1.0.tar.gz
 * we'll look for the big/little endian sequence (short) 8 1 1-or-2 given big/little decision
 *
 * 0: header size in shorts (8 = 16 bytes) (OGI says this is in bytes)
 * 2: version (1)
 * 4: chans
 * 6: rate (srate = 4000000/rate)
 * 8: samples (int) -- seems to be off by 2 -- are they counting ints here?
 * 12: data format (0 = big-endian)
 * 16: data start
*/ 

static int read_adc_header(const char *filename, int chan)
{
  bool little;
  little = (mus_char_to_uninterpreted_int((unsigned char *)(hdrbuf + 12)) != 0); /* 0 = big endian */
  data_location = 16;
  if (little) data_format = MUS_LSHORT; else data_format = MUS_BSHORT;
  chans = big_or_little_endian_short((unsigned char *)(hdrbuf + 4), little);
  srate = 4000000 / big_or_little_endian_short((unsigned char *)(hdrbuf + 6), little);
  data_size = 2 * big_or_little_endian_int((unsigned char *)(hdrbuf + 8), little);
  comment_start = 0;
  comment_end = 0;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ AVR -------------------------------------- 
 *
 *   0: "2BIT"
 *   4: sample name (null padded ASCII)
 *  12: chans (short) (0 = mono, -1 = stereo)
 *  14: sample size (8 or 16 bit) (short) (value is 8, 12, or 16)
 *  16: sample format (signed or unsigned) (short) (0 = unsigned, -1 = signed)
 *  18: loop (on/off), 20: midi (-1 = no MIDI)
 *  22: srate 
 *      avr.txt has:
 *      22: Replay speed 0 = 5.485 Khz, 1 = 8.084 Khz, 2 = 10.971 Khz, 3 = 16.168 Khz, 4 = 21.942 Khz, 5 = 32.336 Khz, 6 = 43.885 Khz, 7 = 47.261 Khz
 *      23: sample rate	in Hertz (as a 3 byte quantity??)
 *  26: length in samples
 *  30: loop beg, 34: loop end, 38: midi (keyboard split), 40: compression, 42: nada ("reserved"), 44: name
 *  64: comment (limited to 64 bytes)
 * 128: data start
 *
 * the Atari .avr files appear to be 8000 Hz, mono, 8-bit linear unsigned data with an unknown header of 128 words
 * apparently there was a change in format sometime in the 90's.
 * 
 * The actual avr files I've found on the net are either garbled, or
 * something is wrong with this definition (taken from CMJ and www.wotsit.org's avr.txt). 
 * SGI dmconvert assumes big-endian here -- this is an Atari format, so it's probably safe to assume big-endian.
 */

static int read_avr_header(const char *filename, int chan)
{
  int dsize, dsigned, i;
  chans = mus_char_to_bshort((unsigned char *)(hdrbuf + 12));
  if (chans == 0) chans = 1; else if (chans == -1) chans = 2; else return(mus_error(MUS_HEADER_READ_FAILED, "%s chans: %d", filename, chans));
  data_location = 128;
  data_size = mus_char_to_bint((unsigned char *)(hdrbuf + 26));
  srate = mus_char_to_ubshort((unsigned char *)(hdrbuf + 24));
  dsize = mus_char_to_bshort((unsigned char *)(hdrbuf + 14));
  dsigned = mus_char_to_bshort((unsigned char *)(hdrbuf + 16));
  if (dsize == 16) 
    {
      if (dsigned == 0)
	data_format = MUS_UBSHORT;
      else data_format = MUS_BSHORT;
    }
  else 
    {
      if (dsize == 8)
	{
	  if (dsigned == 0) 
	    data_format = MUS_UBYTE;
	  else data_format = MUS_BYTE;
	}
      else return(mus_error(MUS_HEADER_READ_FAILED, "%s: unknown data format", filename));
    }
  if (seek_and_read(chan, (unsigned char *)hdrbuf, 64, 64) <= 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s avr header: ran off end of file", filename));
  comment_start = 64;
  i = 0;
  while ((i < 64) && (hdrbuf[i] != 0)) i++;
  comment_end = 64 + (i - 1);
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}




/* ------------------------------------ SNDT -------------------------------------
 *
 * this taken from sndrtool.c (sox-10): (modified 6-Feb-98)
 *   0: "SOUND" (or off by two throughout if not "0x1a"?)
 *   5: 0x1a
 *   6-7: 0
 *   8-11: nsamps (at 12)
 *  12-15: 0
 *  16-19: nsamps
 *  20-21: srate (little endian short) (at 22)
 *  22-23: 0 
 *  24-25: 10
 *  26-27: 4
 *  28-> : <filename> "- File created by Sound Exchange"
 *  .->95: 0 ?
 */

/* similar is Sounder format: 
 * 0: 0
 * 2: short srate (little endian)
 * 4: 10
 * 6: 4
 * then data
 * but this format can't be distinguished from a raw sound file
 */

static int read_sndt_header(const char *filename, int chan)
{
  if (hdrbuf[4] != 'D') return(mus_error(MUS_HEADER_READ_FAILED, "%s: SNDT[4] != 'D'", filename));
  data_format = MUS_UBYTE;
  chans = 1;
  srate = mus_char_to_ulshort((unsigned char *)(hdrbuf + 20));
  data_location = 126;
  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 8));
  if (data_size < 0) data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 10));
  if (srate <= 1) srate = mus_char_to_ulshort((unsigned char *)(hdrbuf + 22));
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Covox v8 ------------------------------------- 
 *
 *  0: 377 125 377 252 377 125 377 252 x x 0's to 16
 * then 8-bit unsigned data
 */

static int read_covox_header(const char *filename, int chan)
{
  data_format = MUS_UBYTE;
  chans = 1;
  data_location = 16;
  srate = 8000;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = true_file_length - data_location;
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  return(MUS_NO_ERROR);
}


/* ------------------------------------ SMP ------------------------------------- 
 *
 *  0: "SOUND SAMPLE DATA "
 * 18: "2.1 "
 * 22-81: comment
 * 82-111: sample name
 * header 112 bytes
 * long samples (bytes = samples*2)
 * then data start
 * data
 * always little endian
 */

static int read_smp_header(const char *filename, int chan)
{
  data_format = MUS_LSHORT;
  chans = 1;
  comment_start = 22;
  comment_end = 81;
  data_location = 116;
  lseek(chan, 112, SEEK_SET);
  if (read(chan, hdrbuf, 4) != 4) return(mus_error(MUS_HEADER_READ_FAILED, "%s: SMP header truncated?", filename));
  data_size = (mus_char_to_lint((unsigned char *)hdrbuf));
  data_format = MUS_LSHORT; /* just a guess */
  srate = 8000; /* docs mention an srate floating around at the end of the file, but I can't find it in any example */
  true_file_length = SEEK_FILE_LENGTH(chan);
  if ((data_size * 2) > true_file_length)
    {
      data_size = (true_file_length - data_location) / 2;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  return(MUS_NO_ERROR);
}


/* ------------------------------------ SPPACK ------------------------------------- 
 * 
 * from AF docs:
 *         Bytes   Type    Contents
 *     0   160    char   Text strings (2 * 80)
 *   160    80    char   Command line
 *   240     2    int    Domain (1-time, 2-freq, 3-qfreq)
 *   242     2    int    Frame size
 *   244     4    float  Sampling frequency
 *   252     2    int    File identifier (i.e. #o100 #o303)
 *   254     2    int    Data type (0xfc0e = sampled data file)
 *   256     2    int    Resolution (in bits 8, 16)
 *   258     2    int    Companding flag
 *   272   240    char   Text strings (3 * 80)
 *   512   ...    --     Audio data
 *
 * at least one program is writing these headers using little endian data...
 */

static int read_sppack_header(const char *filename, int chan)
{
  int typ, bits;
  data_location = 512;
  chans = 1;
  lseek(chan, 240, SEEK_SET);
  if (read(chan, hdrbuf, 22) != 22) return(mus_error(MUS_HEADER_READ_FAILED, "%s SPPACK header truncated?", filename));
  typ = mus_char_to_bshort((unsigned char *)hdrbuf);
  data_format = MUS_UNKNOWN;
  if (typ == 1) 
    {
      if (((hdrbuf[254]) == 252) && ((hdrbuf[255]) == 14)) /* #xfc and #x0e */
	{
	  float sr;
	  typ = mus_char_to_bshort((unsigned char *)(hdrbuf + 18));
	  bits = mus_char_to_bshort((unsigned char *)(hdrbuf + 16));
	  sr = mus_char_to_bfloat((unsigned char *)(hdrbuf + 4));
	  srate = (int)sr;
	  switch (typ)
	    {
	    case 1: if (bits == 16) data_format = MUS_BSHORT; else data_format = MUS_BYTE; break;
	    case 2: data_format = MUS_ALAW; break;
	    case 3: data_format = MUS_MULAW; break;
	    default: data_format = MUS_UNKNOWN; break;
	    }
	  data_size = SEEK_FILE_LENGTH(chan);
	  data_size = mus_bytes_to_samples(data_format, data_size - 512);
	  comment_start = 0;
	  comment_end = 0;
	}
    }
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location) 
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  if (data_size > mus_bytes_to_samples(data_format, true_file_length))
    data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ ESPS (Entropic Signal Processing System) ------------------------------------- 
 *
 * specs at ftp.entropic.com (also known as "SD" format)
 * from AFgetInfoES.c:
 * 
 *       Bytes     Type    Contents
 *      8 -> 11    --     Header size (bytes)
 *     12 -> 15    int    Sampled data record size
 *     16 -> 19    int    File identifier: 0x00006a1a or 0x1a6a0000
 *     40 -> 65    char   File creation date
 *    124 -> 127   int    Number of samples
 *    132 -> 135   int    Number of doubles in a data record
 *    136 -> 139   int    Number of floats in a data record
 *    140 -> 143   int    Number of longs in a data record
 *    144 -> 147   int    Number of shorts in a data record
 *    148 -> 151   int    Number of chars in a data record
 *    160 -> 167   char   User name 
 *    333 -> H-1   --     "Generic" header items, including "record_freq" {followed by a "double8"=64-bit ?}
 *      H -> ...   --     Audio data
 */

static int read_esps_header(const char *filename, int chan)
{
  char str[80];
  bool happy = true;
  off_t curbase, hend;
  int k, j, n, chars, floats, shorts, doubles, bytes;
  bool little;
  little = (hdrbuf[18] == 0);
  if (little)
    data_location = mus_char_to_lint((unsigned char *)(hdrbuf + 8));
  else data_location = mus_char_to_bint((unsigned char *)(hdrbuf + 8));
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = true_file_length - data_location;
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 8000;
  chans = 1;
  lseek(chan, 132, SEEK_SET);
  CHK_READ(chan, hdrbuf, HDRBUFSIZ);
  if (little)
    {
      doubles = mus_char_to_lint((unsigned char *)hdrbuf);
      floats = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
      shorts = mus_char_to_lint((unsigned char *)(hdrbuf + 12));
      chars = mus_char_to_lint((unsigned char *)(hdrbuf + 16));
    }
  else
    {
      doubles = mus_char_to_bint((unsigned char *)hdrbuf);
      floats = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
      shorts = mus_char_to_bint((unsigned char *)(hdrbuf + 12));
      chars = mus_char_to_bint((unsigned char *)(hdrbuf + 16));
    }
  if (shorts != 0)
    {
      data_format = ((little) ? MUS_LSHORT : MUS_BSHORT); 
      chans = shorts;
    }
  else
    {
      if (doubles != 0)
	{
	  data_format = ((little) ? MUS_LDOUBLE_UNSCALED : MUS_BDOUBLE_UNSCALED);
	  chans = doubles;
	}
      else
	{
	  if (floats != 0)
	    {
	      data_format = ((little) ? MUS_LFLOAT_UNSCALED : MUS_BFLOAT_UNSCALED);
	      chans = floats;
	    }
	  else
	    {
	      if (chars != 0)
		{
		  data_format = MUS_BYTE; /* ?? */
		  chans = chars;
		}
	    }
	}
    }
  /* search for "record_freq" to get srate */
  lseek(chan, 333, SEEK_SET);
  CHK_READ(chan, hdrbuf, HDRBUFSIZ);
  curbase = 333;
  hend = curbase + HDRBUFSIZ;
  k = 0;
  n = 0;
  for (j = 0; j < 80; j++) str[j] =' ';  
  while (happy) 
    {
      str[k] = hdrbuf[n];
      if ((str[k] == 'q') || (str[k] == 3) || ((curbase + n + 1) >= data_location) || (k == 78))
	{ /* 3 = C-C marks end of record (?) */
	  str[k + 1] = 0;
	  if (strcmp(str, "record_freq") == 0) 
	    {
	      if (seek_and_read(chan, (unsigned char *)hdrbuf, curbase + n, 32) <= 0)
		return(mus_error(MUS_HEADER_READ_FAILED, "%s esps header: ran off end of file", filename));
	      n = 0;
	      if (little)
		srate = (int)mus_char_to_ldouble((unsigned char *)(hdrbuf + 8));
	      else srate = (int)mus_char_to_bdouble((unsigned char *)(hdrbuf + 8));
	      happy = false;
	    }
	  if ((curbase + n + 1) >= data_location) happy = false;
	  k = 0;
	}
      else
	k++;
      n++;
      if (n >= hend)
	{
	  curbase += hend;
	  n = 0;
	  bytes = read(chan, hdrbuf, HDRBUFSIZ);
	  if (bytes != HDRBUFSIZ) break;
	  hend = HDRBUFSIZ;
	}
    }
  if (srate == 0) srate = 8000;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

 
 
/* ------------------------------------ INRS ------------------------------------- 
 * 
 *   from AFgetInfoIN.c:
 * 
 *    INRS-Telecommunications audio file:
 *       Bytes     Type    Contents
 *      0 ->  3    float  Sampling Frequency (VAX float format)
 *      6 -> 25    char   Creation time (e.g. Jun 12 16:52:50 1990)
 *     26 -> 29    int    Number of speech samples in the file (? -- old INRS files omit this)
 *   The data in an INRS-Telecommunications audio file is in 16-bit integer (little-endian)
 *   format. Header is always 512 bytes.  Always mono.
 * 
 */

static int inrs_srates[NINRS] = {6500, 6667, 8000, 10000, 12000, 16000, 20000};

static int read_inrs_header(const char *filename, int chan, int loc)
{
  true_file_length = SEEK_FILE_LENGTH(chan);
  comment_start = 6;
  comment_end = 25;
  data_format = MUS_LSHORT;
  srate = loc;
  chans = 1;
  data_location = 512;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location) 
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ MAUD ------------------------------------- 
 *
 * very similar to AIFF:
 *  "MHDR" => 4: chunksize (32)
 *            8: samples 
 *           12: bits 
 *           14: ditto
 *           16: clock freq
 *           20: clock div (srate = freq/div)
 *           22: chan info (0 = mono, 1 = stereo)
 *           24: ditto(?!)
 *           26: format (0 = unsigned 8 or signed 16 (see bits), 2 = alaw, 3 = mulaw)
 *           28-40: unused
 *  "MDAT" => data
 *  "ANNO" => comment
 */

static int read_maud_header(const char *filename, int chan)
{
  int chunksize, offset, chunkloc;
  bool happy;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
  chunkloc = 12;
  offset = 0;
  data_format = MUS_BYTE;
  srate = 0;
  chans = 1;
  happy = true;
  update_form_size = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
  while (happy)
    {
      offset += chunkloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 32) <= 0)
	return(mus_error(MUS_HEADER_READ_FAILED, "%s maud header: chunks confused at %d", filename, offset));
      chunksize = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
      if ((chunksize == 0) && /* can be empty data chunk? */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if (match_four_chars((unsigned char *)hdrbuf, I_MHDR))
	{
	  int num, den;
	  data_size = mus_char_to_bint((unsigned char *)(hdrbuf + 8));
	  num = mus_char_to_bint((unsigned char *)(hdrbuf + 16));
	  den = mus_char_to_bshort((unsigned char *)(hdrbuf + 20));
	  if (den == 0) den = 1;
	  srate = (int)(num / den);
	  num = mus_char_to_bshort((unsigned char *)(hdrbuf + 12));
	  den = mus_char_to_bshort((unsigned char *)(hdrbuf + 26));
	  if (num == 8)
	    {
	      switch (den)
		{
		case 0: data_format = MUS_UBYTE; break;
		case 2: data_format = MUS_ALAW; break;
		case 3: data_format = MUS_MULAW; break;
		default: data_format = MUS_UNKNOWN; break;
		}
	    }
	  else data_format = MUS_BSHORT;
	  num = mus_char_to_bshort((unsigned char *)(hdrbuf + 22));
	  if (num == 0) chans = 1; else chans = 2;
	}
      else
	{
	  if (match_four_chars((unsigned char *)hdrbuf, I_ANNO))
	    {
	      comment_start = offset + 8;
	      comment_end = comment_start + chunksize - 1;
	    }
	  else
	    {
	      if (match_four_chars((unsigned char *)hdrbuf, I_MDAT))
		{
		  data_location = offset + 12;
		  happy = false;
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++; /* extra null appended to odd-length chunks */
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no MDAT chunk?", filename));
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ CSL ------------------------------------- 
 *
 * "Computerized Speech Labs -- this info taken from wavesurfer/snack
 *
 * very similar to AIFF:
 * 0: FORM
 * 4: DS16 (kinda weird)
 * 8: size (int le)
 * 12: chunks
 *     HEDR or HDR8
 *     4: size (int)
 *     samp: short, chans: 1 at 36 if not (int) -1, chans: 2?
 *     srate at 28 (le int?)
 * other chunks: SD_B, SDA_ SDAB with data bytes as data followed by data
 */

static int read_csl_header(const char *filename, int chan)
{
  int chunksize, offset, chunkloc;
  bool happy;
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
  chunkloc = 12;
  offset = 0;
  data_format = MUS_LSHORT;
  srate = 0;
  chans = 1;
  happy = true;
  update_form_size = mus_char_to_lint((unsigned char *)(hdrbuf + 8));
  while (happy)
    {
      offset += chunkloc;
      if (seek_and_read(chan, (unsigned char *)hdrbuf, offset, 64) <= 0)
	return(mus_error(MUS_HEADER_READ_FAILED, "%s csl header: chunks confused at %d", filename, offset));
      chunksize = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
      if ((chunksize == 0) && /* can be empty data chunk? */
	  (hdrbuf[0] == 0) && (hdrbuf[1] == 0) && (hdrbuf[2] == 0) && (hdrbuf[3] == 0))
	break;
      if ((match_four_chars((unsigned char *)hdrbuf, I_HEDR)) || 
	  (match_four_chars((unsigned char *)hdrbuf, I_HDR8)))
	{
	  /* 8..20: date as ascii */
	  /* 32: data length (int) in bytes */
	  if ((mus_char_to_lshort((unsigned char *)(hdrbuf + 36)) != -1) && /* these are maxamps, -1=none */
	      (mus_char_to_lshort((unsigned char *)(hdrbuf + 38)) != -1))
	    chans = 2;
	  srate = mus_char_to_lint((unsigned char *)(hdrbuf + 28));
	}
      else
	{
	  if (match_four_chars((unsigned char *)hdrbuf, I_NOTE))
	    {
	      comment_start = offset + 8;
	      comment_end = comment_start + chunksize - 1;
	    }
	  else
	    {
	      if ((match_four_chars((unsigned char *)hdrbuf, I_SDA_)) ||
		  (match_four_chars((unsigned char *)hdrbuf, I_SDAB)) ||
		  (match_four_chars((unsigned char *)hdrbuf, I_SD_B)))
		{
		  data_location = offset + 8;
		  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 4));
		  happy = false;
		}
	    }
	}
      chunkloc = (8 + chunksize);
      if (chunksize & 1) chunkloc++;
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no SDxx chunk?", filename));
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ snack SMP ------------------------------------- 
 *
 * there's apparently yet another "smp" format (from nist??)
 * file = samp
 * sftot = 22050
 * msb = last
 * nchans = 1
 * preemph = none
 * born = snack
 * msb = last here -> little endian?
 * data at 1024
 */

static int read_file_samp_header(const char *filename, int chan)
{
  int i = 0;
  char *locbuf;
  data_location = 1024;
  chans = 1;
  srate = 8000;
  data_format = MUS_LSHORT;
  lseek(chan, 10, SEEK_SET);
  locbuf = (char *)CALLOC(1024, sizeof(char));
  CHK_READ(chan, locbuf, 1024);
  while (i < 1024)
    {
      if (strncmp((char *)(locbuf + i), "sftot", 5) == 0)
	sscanf((const char *)(&locbuf[i + 6]), "%d", &srate);
      if (strncmp((char *)(locbuf + i), "nchans", 6) == 0)
	sscanf((const char *)(&locbuf[i + 7]), "%d", &chans);
      if (strncmp((char *)(locbuf + i), "msb", 3) == 0)
	if (strncmp((char *)(locbuf + i + 4), "first", 5) == 0)
	  data_format = MUS_BSHORT;
      while ((i < 1024) && (locbuf[i] != 10) && (locbuf[i] != 0)) i++;
      i++;
    }
  FREE(locbuf);
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Sound Designer I -------------------------------------
 *
 * complicated and defined in terms of Pascal records, so the following is a stab in the dark:
 *
 * 0:    1336 (i.e. header size)
 * 764:  comment (str255)
 * 1020: sample rate (long)
 * 1028: data size (short)
 * 1030: a code string describing the data type (i.e. "linear") (str32)
 * 1064: user comment (str255)
 *
 * file type: 'SFIL'
 *
 * always big_endian
 */

static int read_sd1_header(const char *filename, int chan)
{
  int n;
  chans = 1;
  data_location = 1336;
  lseek(chan, 1020, SEEK_SET);
  if (read(chan, hdrbuf, 64) != 64) return(mus_error(MUS_HEADER_READ_FAILED, "%s Sound Designer I header truncated?", filename));
  srate = mus_char_to_bint((unsigned char *)hdrbuf);
  n = mus_char_to_bshort((unsigned char *)(hdrbuf + 8));
  if (n == 16)
    data_format = MUS_BSHORT;
  else data_format = MUS_BYTE;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  n = ((unsigned char)hdrbuf[44]);
  if (n != 0) 
    {
      comment_start = 1064;
      comment_end = comment_start + n - 1;
    }
  return(MUS_NO_ERROR);
}



/* ------------------------------------ PSION alaw -------------------------------------
 *
 * 0: "ALawSoundFile**"
 * 16: version 
 * 18: length (bytes)
 * 22: padding
 * 24: repeats
 * 26-32: nada
 * 32: data
 *
 * always mono 8-bit alaw 8000 Hz. All the examples on the psion net site appear to be little endian.
 */

static int read_psion_header(const char *filename, int chan)
{
  if ((hdrbuf[13] != '*') || (hdrbuf[14] != '*')) return(mus_error(MUS_HEADER_READ_FAILED, "%s: PSION[13, 14] != '*'", filename));
  chans = 1;
  data_location = 32;
  srate = 8000;
  data_format = MUS_ALAW;
  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 18)); /* always little-endian? */
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ Gravis Ultrasound Patch -------------------------------------
 *
 * http://www.gravis.com/Public/sdk/PATCHKIT.ZIP
 *
 * header [128], instruments [62], layers [49], waveheaders (nested)
 * always little endian, actual files don't match exactly with any documentation
 *
 * Header block:
 *   0:  "GF1PATCH100" or "GF1PATCH110"
 *   12: "ID#000002"
 *   22: comment (copyright notice) (60 bytes ASCIZ)
 *   82: number of instruments
 *   83: number of voices
 *   84: wave channels
 *   85: number of waves
 *   87: vol
 *   89: size? 
 *   93: reserved (36? bytes)
 *
 * Instrument block:
 *   0: id
 *   2: name (16 bytes)
 *   18: size
 *   22: number of layers
 *   23: reserved (40? bytes)
 *
 * Layer block:
 *   0: "previous"
 *   1: id
 *   2: size
 *   6: number of wave samples
 *  10: reserved (40? bytes)
 *
 * Wave block:
 *   0: name (7 bytes ASCIZ)
 *   7: "fractions"
 *   8: data size of wave sample
 *  12: loop start
 *  16: loop end
 *  20: sample rate
 *  22: low freq
 *  26: high freq
 *  30: root freq
 *  34: tune
 *  36: balance
 *  37: envelope data (6+6 bytes I think)
 *  49: tremolo and vibrato data (6 bytes)
 *  55: mode bit 0: 8/16, 1: signed/unsigned
 *  56: scale freq
 *  58: scale factor
 *  60: reserved (36 bytes)
 *  followed by data presumably
 */

static int read_gravis_header(const char *filename, int chan)
{
  int mode;
  lseek(chan, 0, SEEK_SET);
  if (read(chan, hdrbuf, 128) != 128) return(mus_error(MUS_HEADER_READ_FAILED, "%s PAT header truncated?", filename));
  chans = hdrbuf[84];
  if (chans == 0) chans = 1;
  comment_start = 22;
  comment_end = 81;
  lseek(chan, 239, SEEK_SET); /* try to jump to wave sample block (128+62+49) */
  CHK_READ(chan, hdrbuf, 128);
  srate = mus_char_to_ulshort((unsigned char *)(hdrbuf + 20));
  data_size = mus_char_to_ulshort((unsigned char *)(hdrbuf + 8));
  mode = hdrbuf[55];
  if (mode & 1)
    {
      if (mode & 2)
	data_format = MUS_ULSHORT;
      else data_format = MUS_LSHORT;
    }
  else
    {
      if (mode & 2)
	data_format = MUS_UBYTE;
      else data_format = MUS_BYTE;
    }
  data_location = 337;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ Goldwave -------------------------------------
 *
 * http://web.cs.mun.ca/~chris3/goldwave/goldwave.html
 */

static int read_goldwave_header(const char *filename, int chan)
{
  chans = 1;
  data_location = 28;
  data_format = MUS_LSHORT;
  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 22));
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  if ((data_size <= 24) || (data_size > true_file_length))
    data_size = (true_file_length - data_location) / 2;
  else data_size /= 2;
  srate = mus_char_to_lint((unsigned char *)(hdrbuf + 18));
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Sonic Resource Foundry -------------------------------------
 *
 * more reverse engineering...
 * http://www.sfoundry.com/
 */

static int read_srfs_header(const char *filename, int chan)
{
  chans = 1; /* might be short at header[4] */
  data_location = 32;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = (true_file_length - data_location) / 2;
  srate = mus_char_to_lint((unsigned char *)(hdrbuf + 6));
  data_format = MUS_LSHORT;
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Quicktime -------------------------------------
 *
 * infinitely complicated -- see Quicktime File Format doc from Apple.
 * there's no relation between this document and actual files -- a bizarre joke?
 */

static int read_qt_header(const char *filename, int chan)
{
  chans = 1;
  data_location = 12;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 11025; /* ?? */
  data_format = MUS_UBYTE;
  return(MUS_NO_ERROR);
}


/* ------------------------------------ SBStudioII -------------------------------------
 *
 * from a file created by Convert 1.4
 * 0: SND <space>
 * 8: file size - 8
 * SNNA SNIN SNDT blocks:
 *
 * built in blocks, other names are SNIN, SNDT
 * need to scan for SNDT, block length, data
 * SNNA len name 
 * supposedly ends with END (but my examples don't)
 * SNIN: 
 *   num (2), reserved (2), tuning (1), vol (2), type (2) bit 0: 1 = PCM, bit 1: 1 = 16, 0 = 8 (then loop data)
 * info from Pac.txt (pac.zip) at http://www.wotsit.org/music.htm 
 */

static int read_sbstudio_header(const char *filename, int chan)
{
  int i, tmp;
  bool happy;
  unsigned char *bp;
  lseek(chan, 0, SEEK_SET);
  CHK_READ(chan, hdrbuf, HDRBUFSIZ);
  chans = 1; 
  srate = 8000; /* no sampling rate field in this header */
  data_format = MUS_UNKNOWN;
  true_file_length = SEEK_FILE_LENGTH(chan);
  happy = true;
  i = 8;
  bp = (unsigned char *)(hdrbuf + 8);
  while (happy)
    {
      if (match_four_chars(bp, I_SNDT))
	{
	  data_size = mus_char_to_lint((unsigned char *)(bp + 4));
	  data_location = i + 8;
	  happy = false;
	}
      else
	{
	  if (match_four_chars(bp, I_SNIN))
	    {
	      tmp = mus_char_to_lshort((unsigned char *)(bp + 15));
	      if ((tmp & 1) == 0) 
		data_format = MUS_UNKNOWN;
	      else
		{
		  if ((tmp & 2) == 0) 
		    data_format = MUS_BYTE;
		  else data_format = MUS_LSHORT;
		}
	      i += 26;
	      bp += 26;
	    }
	  else
	    {
	      if (match_four_chars(bp, I_SNNA))
		{
		  tmp = mus_char_to_lint((unsigned char *)(bp + 4));
		  i += tmp;
		  bp += tmp;
		}
	      else
		{
		  i++;
		  bp++;
		}
	    }
	}
      if (i >= HDRBUFSIZ)
	{
	  data_format = MUS_UNKNOWN;
	  happy = false;
	}
    }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no SNDT chunk?", filename));
  if ((data_size == 0) || (data_format == MUS_UNKNOWN)) 
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data size or format bogus", filename));
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Delusion Sound -------------------------------------
 *
 * more reverse engineering...
 * from a file created by Convert 1.4
 * 0: DDSF
 * 5: name (text)
 * 55: data
 * probaby similar to DMF format described in Dmf-form.txt but I don't see any other block names in the data
 */

static int read_delusion_header(const char *filename, int chan)
{
  if ((hdrbuf[4] != 1) || (hdrbuf[5] > 128) || (hdrbuf[6] > 128) || (hdrbuf[7] > 128)) 
    return(mus_error(MUS_HEADER_READ_FAILED, "%s DDSF name bogus", filename));
  chans = 1; 
  data_location = 55;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 8000;
  data_format = MUS_LSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ Farandole Composer WaveSample -------------------------------------
 *
 * 0: FSM 254
 *    libmodplug load_far.cpp uses: #define FARFILEMAGIC	0xFE524146 ("FAR="?)
 * 4: name (text) (32 bytes)
 * 36: 10, 13, 26 or something like that
 * 39: len?
 * 40: volume
 * 41: looping data
 * 49: type (0 = 8-bit, else 16)
 * 50: loop mode
 * 51: data
 * described in Fsm.txt and Far-form.txt http://www.wotsit.org/music.htm 
 */

static int read_farandole_header(const char *filename, int chan)
{
  chans = 1; 
  data_location = 51;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 8000;
  if (hdrbuf[49] == 0)
    data_format = MUS_BYTE;
  else data_format = MUS_LSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ Yamaha TX-16W -------------------------------------
 *
 * ftp://ftp.t0.or.at/pub/sound/tx16w/samples.yamaha
 * ftp://ftp.t0.or.at/pub/sound/tx16w/faq/tx16w.tec
 * http://www.t0.or.at/~mpakesch/tx16w/
 *
 * from tx16w.c sox 12.15: (7-Oct-98) (Mark Lakata and Leigh Smith)
 *  char filetype[6] "LM8953"
 *  nulls[10],
 *  dummy_aeg[6]
 *  format 0x49 = looped, 0xC9 = non-looped
 *  sample_rate 1 = 33 kHz, 2 = 50 kHz, 3 = 16 kHz 
 *  atc_length[3] if sample rate 0, [2]&0xfe = 6: 33kHz, 0x10:50, 0xf6: 16, depending on [5] but to heck with it
 *  rpt_length[3] (these are for looped samples, attack and loop lengths)
 *  unused[2]
 */

static int read_tx16w_header(const char *filename, int chan)
{
  if ((hdrbuf[4] != '5') || (hdrbuf[5] != '3')) return(mus_error(MUS_HEADER_READ_FAILED, "%s TX16 magic number bogus", filename));
  chans = 1; 
  data_location = 32;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 16000;
  if (hdrbuf[23] == 1) srate = 33000;
  else if (hdrbuf[23] == 2) srate = 50000;
  else if (hdrbuf[23] == 3) srate = 16000;
  else if (hdrbuf[23] == 0)
    {
      if ((hdrbuf[26] & 0xFE) == 6) srate = 33000;
      else if ((hdrbuf[26] & 0xFE) == 0x10) srate = 50000;
      else if ((hdrbuf[26] & 0xFE) == 0xf6) srate = 16000;
    }
  original_data_format = MUS_UNKNOWN;
  data_format = MUS_UNKNOWN;
  data_size = (off_t)((double)data_size / 1.5);
  if (hdrbuf[22] == 0x49)
    {
      loop_modes[0] = 1;
      loop_starts[0] = ((hdrbuf[26] & 1) << 16) + (hdrbuf[25] << 8) + hdrbuf[24];
      loop_ends[0] = loop_starts[0] + ((hdrbuf[29] & 1) << 16) + (hdrbuf[28] << 8) + hdrbuf[27];
    }
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Yamaha SY-85 and SY-99 -------------------------------------
 *
 * more reverse engineering...
 * 0: SY85 (SY80 is SY-99) SY85ALL SY80 SYALL
 * 5: name ("WAVE1")
 * (26 int len)
 * (33: comment or prompt?)
 * data in 16-bit little endian (?)
 */

static int read_sy85_header(const char *filename, int chan)
{
  if ((hdrbuf[4] != ' ') && (hdrbuf[4] != 'A')) return(mus_error(MUS_HEADER_READ_FAILED, "%s: unknown magic number", filename));
  chans = 1; 
  data_location = 1024;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 8000; /* unknown */
  data_format = MUS_BSHORT; /* not right */
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Kurzweil 2000 -------------------------------------
 * 
 * "PRAM" then header len as big endian int??
 * from krz2tx.c (Mark Lakata)
 */
static int read_kurzweil_2000_header(const char *filename, int chan)
{
  chans = 1; 
  data_location = mus_char_to_bint((unsigned char *)(hdrbuf + 4));
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 44100; /* unknown */
  data_format = MUS_BSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Korg -------------------------------------
 * 
 * "SMP1" -- guessing on the rest
 */
static int read_korg_header(const char *filename, int chan)
{
  chans = 1; 
  data_location = 70;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = mus_char_to_bint((unsigned char *)(hdrbuf + 48));
  data_format = MUS_BSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Maui -------------------------------------
 * 
 * "Maui" -- guessing on the rest
 */
static int read_maui_header(const char *filename, int chan)
{
  lseek(chan, 420, SEEK_SET);
  if (read(chan, hdrbuf, 64) != 64) return(mus_error(MUS_HEADER_READ_FAILED, "%s truncated maui header?", filename));
  chans = 1; 
  data_location = 776;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 8));
  if ((data_size * 2) > true_file_length)
    data_size = (true_file_length - data_location) / 2;
  srate = mus_char_to_lint((unsigned char *)(hdrbuf));
  data_format = MUS_LSHORT;
  return(MUS_NO_ERROR);
}

/* ------------------------------------ Impulse Tracker -------------------------------------
 * 
 * data from its2raw.c by Ben Collver
 * 0:  IMPS
 * 4:  filename (12 bytes)
 * 17: global vol
 * 18: flags (1: 16-bit or 8(0), 2: stereo or mono(0)
 * 19: default vol
 * 20: sample name (26 bytes)
 * 46: convert
 * 47: default pan
 * 48: length (samps)
 * 52: loop start
 * 56: loop end
 * 60: srate
 * 64: sustain loop start
 * 68: sustain loop end
 * 72: data location
 * 76: vib speed
 * 77: vib depth
 * 78: vib wave
 * 79: vib rate
 */
static int read_impulsetracker_header(const char *filename, int chan)
{
  if (hdrbuf[18] & 4) chans = 2; else chans = 1;
  if (hdrbuf[18] & 2) data_format = MUS_LSHORT; else data_format = MUS_BYTE;
  data_location = mus_char_to_lint((unsigned char *)(hdrbuf + 72));
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = (true_file_length - data_location);
  srate = mus_char_to_lint((unsigned char *)(hdrbuf + 60));
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

#if 0
/* ------------------------------------ AKAI 3? -------------------------------------
 */
static int read_akai3_header(const char *filename, int chan)
{
  chans = 1;
  data_location = 192;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  if (hdrbuf[1] == 0) srate = 22050; else srate = 44100;
  data_format = MUS_LSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}
#endif


/* ------------------------------------ AKAI 4 -------------------------------------
 * 
 * 1, 4, info from Paul Kellet -- lost the url ("MPC-2000")
 */
static int read_akai4_header(const char *filename, int chan)
{
  chans = hdrbuf[21] + 1;
  data_location = 42;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = mus_char_to_ulshort((unsigned char *)(hdrbuf + 40));
  data_format = MUS_LSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ PVF (portable voice format) -------------------------------------
 *
 * info from mgetty-voice-1.1.22/voice/libpvf/lib.c
 * this is a modem-related interchange format
 * 
 * PVF1\n
 * 1 11025 32\n
 * then data
 * PVF1 = binary data, PVF2 = ascii
 * chans | srate | sample size
 */

static int read_pvf_header(const char *filename, int chan)
{
  char *buf;
  int bits, i;
  if (hdrbuf[4] != '\n') return(mus_error(MUS_HEADER_READ_FAILED, "PVF header messed up"));
  type_specifier = mus_char_to_uninterpreted_int((unsigned char *)hdrbuf);
  buf = (char *)(hdrbuf + 5);
  sscanf(buf, "%d %d %d", &chans, &srate, &bits);
  if (chans < 1) chans = 1;
  if (srate < 0) srate = 8000;
  if (bits < 8) bits = 8;
  for (i = 6; i < INITIAL_READ_SIZE; i++)
    if (hdrbuf[i] == '\n')
      {
	data_location = i + 1;
	break;
      }
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s PVF header bad data location", filename));
  if (match_four_chars((unsigned char *)hdrbuf, I_PVF2))
    {
      data_format = MUS_UNKNOWN; /* ascii text */
      return(mus_error(MUS_HEADER_READ_FAILED, "%s PVF header unknown data format", filename));
    }
  /* big endian data -- they're using htonl etc */
  if (bits == 8)
    data_format = MUS_BYTE;
  else
    if (bits == 16)
      data_format = MUS_BSHORT;
    else data_format = MUS_BINT;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Ultratracker WaveSample -------------------------------------
 *
 * 0..31: name (32 = ctrl-Z?) 
 * 33: PMUWFD (but docs say this is "dos name" -- perhaps we can't recognize this header type reliably)
 * 44: 4 ints giving loop and size data
 * 60: vol
 * 61: "bidi" 0|8|24->8 bit else 16 -- but actual example has 0 with 16-bit
 * 62: finetune
 * 64: data (or 68?)
 * described in Ult-form.txt http://www.wotsit.org/music.htm 
 */

static int read_ultratracker_header(const char *filename, int chan)
{
  chans = 1; 
  data_location = 64;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  data_size = (true_file_length - data_location);
  srate = 8000;
  data_format = MUS_LSHORT;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ Sample dump exchange -------------------------------------
 *
 * 0: SDX:
 * sdx2tx.c (Mark Lakata) reads from 4 for 26 (^z), then
 * version (1)
 * comment as pascal-style string (byte len, bytes chars)
 * then 23 bytes:
 *  0: packing (0 = pcm)
 *  1: midi channel
 *  2 + 256*[3]: sample number
 *  4: sample format (15: 16 bit unsigned(?), 8: 8bit unsigned(?)
 *  5: sample rate (big int?)
 *  9: sample length
 * 13: loop start
 * 17: loop end
 * 21: loop type 
 * 22: reserved
 */

static int read_sample_dump_header(const char *filename, int chan)
{
  int i, len;
  for (i = 4; i < HDRBUFSIZ; i++) if (hdrbuf[i] == 26) break;
  len = hdrbuf[i + 2];
  if (len > 0)
    {
      comment_start = i + 3;
      comment_end = i + 3 + len;
      }
  seek_and_read(chan, (unsigned char *)hdrbuf, i + 3 + len, HDRBUFSIZ);
  srate = mus_char_to_lint((unsigned char *)(hdrbuf + 5));
  loop_modes[0] = 0;
  if (hdrbuf[21] == 0)
    {
      loop_modes[0] = 1;
      loop_starts[0] = mus_char_to_lint((unsigned char *)(hdrbuf + 13));
      loop_ends[0] = mus_char_to_lint((unsigned char *)(hdrbuf + 17));
    }
  /* data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 9)); */
  if ((srate < 100) || (srate > 100000)) srate = 8000;
  chans = 1; 
  data_location = i + 3 + len + 23;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  if (hdrbuf[0] == 0)
    data_format = MUS_ULSHORT;
  else data_format = MUS_UNKNOWN;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Digiplayer ST3 -------------------------------------
 *
 * 0: 1 (use 'SCRS' at 76)
 * 1: name
 * 13: nada
 * 14: "paragraph" offset of sample data
 * 16: length in bytes (looks like #samples in the actual files...)
 * 20: loop start
 * 24: loop end
 * 28: vol
 * 29: ?
 * 30: 0 = unpacked, 1 = DP30ADPCM
 * 31: bits: 0 = loop, 1 = stereo (chans not interleaved!), 2 = 16-bit samples (little endian)
 * 32: freq
 * 36: nada
 * 40: nada
 * 42: 512
 * 44: date?
 * 48: sample name (28 char ASCIZ)
 * 76: 'SCRS'
 * 80: data starts
 *
 * info from http://www.wotsit.org/ S3m-form.txt
 */

static int read_digiplayer_header(const char *filename, int chan)
{
  chans = 1; 
  data_location = 80;
  true_file_length = SEEK_FILE_LENGTH(chan);
  data_size = (true_file_length - data_location);
  if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
  srate = 8000;
  data_format = MUS_ULSHORT;
  if (hdrbuf[30] & 2) chans = 2;
  if (hdrbuf[30] & 1) 
    data_format = MUS_UNKNOWN;
  else
    {
      if (hdrbuf[30] & 4) data_format = MUS_UBYTE; /* may be backwards -- using Convert 1.4 output here */
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ CSRE adf -------------------------------------
 *
 * Info from Stuart Rosen
 *
 * 0-7: CSRE40
 * 8:   samples in file (long)
 * 12:  center line(?) (long)
 * 16:  start channel(?) (unsigned)
 * 18:  bits -- 12 or 16 (unsigned) -- is 12 bit sample file packed?
 * 20:  number system (0 = signed, 1 = unsigned)
 * 22:  srate in kHz (float)
 * 26:  peak sample in file (long) (can be 0)
 * 30-511: comment possibly
 *
 * probably always little-endian (S.R. reads each sample using sizeof(int) -> 16 bits I think)
 * if 12-bit unsigned we need to handle the offset somewhere
 */

static int read_adf_header(const char *filename, int chan)
{
  int bits, numsys;
  lseek(chan, 0, SEEK_SET);
  if ((hdrbuf[4] != '4') || (hdrbuf[5] != '0')) return(mus_error(MUS_HEADER_READ_FAILED, "%s csre header bad magic number", filename));
  if (read(chan, hdrbuf, 30) != 30) return(mus_error(MUS_HEADER_READ_FAILED, "%s csre header truncated?", filename));
  chans = 1;
  numsys = mus_char_to_ulshort((unsigned char *)(hdrbuf + 20));
  bits = mus_char_to_ulshort((unsigned char *)(hdrbuf + 18));
  if ((bits == 16) || (bits == 12))
    {
      if (numsys == 0)
	data_format = MUS_LSHORT;
      else data_format = MUS_ULSHORT;
    }
  else data_format = MUS_UNKNOWN;
  srate = (int)(1000 * mus_char_to_lfloat((unsigned char *)(hdrbuf + 22)));
  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 8));
  data_location = 512;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  if (data_size > mus_bytes_to_samples(data_format, true_file_length - data_location))
    data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ Diamondware -------------------------------------
 * 
 * info from Keith Weiner at DiamondWare (www.dw.com):
 *
 * 0-22:   DWD Header Byte "DiamondWare Digitized\n\0" 
 * 23:     1A (EOF to abort printing of file) 
 * 24:     Major version number 
 * 25:     Minor version number 
 * 26-29:  Unique sound ID (checksum XOR timestamp) 
 * 30:     Reserved 
 * 31:     Compression type (0 = none) 
 * 32-33:  Sampling rate (in Hz) 
 * 34:     Number of channels (1 = mono, 2 = stereo) (interleaved)
 * 35:     Number of bits per sample (8, 16) (all data signed)
 * 36-37:  Absolute value of largest sample in file 
 * 38-41:  length of data section (in bytes) 
 * 42-45:  # samples (16-bit stereo is 4 bytes/sample) 
 * 46-49:  Offset of data from start of file (in bytes) 
 * 50-53:  Reserved for future expansion (markers) 
 * 54-55:  Padding 
 * 56:offset -- additional text: field = value
 *  suggested fields: TITLE, ORGARTIST, GENRE, KEYWORDS, ORGMEDIUM, EDITOR, DIGITIZER, COMMENT, SUBJECT, COPYRIGHT, SOFTWARE, CREATEDATE
 *
 * since this is all Windows/DOS oriented, I'll assume little-endian byte order.
 */

static int read_diamondware_header(const char *filename, int chan)
{
  lseek(chan, 0, SEEK_SET);
  if (read(chan, hdrbuf, 64) != 64) return(mus_error(MUS_HEADER_READ_FAILED, "%s truncated diamondware header?", filename));
  chans = hdrbuf[34];
  if (hdrbuf[31] == 0)
    {
      if (hdrbuf[35] == 8) data_format = MUS_BYTE;
      else data_format = MUS_LSHORT;
    }
  else 
    {
      data_format = MUS_UNKNOWN;
      return(mus_error(MUS_HEADER_READ_FAILED, "%s unknown data format", filename));
    }
  srate = mus_char_to_ulshort((unsigned char *)(hdrbuf + 32));
  data_size = mus_char_to_lint((unsigned char *)(hdrbuf + 38));
  data_location = mus_char_to_lint((unsigned char *)(hdrbuf + 46));
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  if (data_size > true_file_length - data_location)
    data_size = true_file_length - data_location;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

/* ------------------------------------ Ensoniq Paris -------------------------------------
 * _paf -> Ensoniq Paris?  (this info from libaudiofile)
 *  0   paf (or fap)
 *  4  version (0)
 *  8  endianess (0 = big)
 * 12  rate (unsigned int)
 * 16  format (0: 16-bit linear, 24-bit linear)
 * 20  channels
 * 24  source (an encoded comment)
 * 2048 data (24 bit files are compressed)
 */

static int read_paf_header(const char *filename, int chan)
{
  int form;
  bool little = false;
  lseek(chan, 0, SEEK_SET);
  if (read(chan, hdrbuf, 32) != 32) return(mus_error(MUS_HEADER_READ_FAILED, "%s PAF header truncated?", filename));
  data_format = MUS_UNKNOWN;
  if (mus_char_to_bint((unsigned char *)(hdrbuf + 8))) little = true;
  if (little)
    {
      srate = mus_char_to_ulint((unsigned char *)(hdrbuf + 12));
      form = mus_char_to_ulint((unsigned char *)(hdrbuf + 16));
      if (form == 0) data_format = MUS_LSHORT;
      if (form == 2) data_format = MUS_BYTE;
      chans = mus_char_to_ulint((unsigned char *)(hdrbuf + 20));
    }
  else
    {
      srate = mus_char_to_ubint((unsigned char *)(hdrbuf + 12));
      form = mus_char_to_ubint((unsigned char *)(hdrbuf + 16));
      if (form == 0) data_format = MUS_BSHORT;
      if (form == 2) data_format = MUS_BYTE;
      chans = mus_char_to_ubint((unsigned char *)(hdrbuf + 20));
    }
  data_location = 2048;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (true_file_length < data_location)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_location " OFF_TD " > file length: " OFF_TD, filename, data_location, true_file_length));
  if (data_format != MUS_UNKNOWN) 
    data_size = mus_bytes_to_samples(data_format, true_file_length - 2048);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ Comdisco SPW -------------------------------------
 * info from AFsp libtsp/AF/nucleus/AFgetSWpar.c
 *
 * header is text as in NIST:
 *
 *   $SIGNAL_FILE 9\n (12 chars)
 *   $USER_COMMENT
 *   <comment line(s)>
 *   $COMMON_INFO
 *   SPW Version        = 3.10
 *   System Type        = <machine> (e.g. "sun4", "hp700")
 *   Sampling Frequency = <Sfreq>   (e.g. "8000")
 *   Starting Time      = 0
 *   $DATA_INFO
 *   Number of points   = <Nsamp>   (e.g. "2000")
 *   Signal Type        = <type>    ("Double", "Float", "Fixed-point", "Integer", "Logical")
 *   Fixed Point Format = <16, 0, t> <16, 16, t> <8, 8, t> <8, 0, t> (optional)
 *   Complex Format     = Real_Imag (optional)
 *   $DATA <data_type>              ("ASCII", "BINARY")
 *
 * the fixed point <n, m, b> is decoded as n = number of bits total per sample, m = integer bits, b = t: signed, u: unsigned
 * if $DATA ASCII, data is ascii text as in IEEE text files.
 * There are other complications as well.  We'll just hack up a stop-gap until someone complains.
 */

static int read_comdisco_header(const char *filename, int chan)
{
  /* need to grab a line at a time, call strcmp over and over.  This is very tedious. */
  char *line = NULL;
  int i, j, k, m, n, curend, offset, len, type, d_size = 0;
  bool happy = true, little, commenting;
  k = 15;
  line = (char *)CALLOC(256, sizeof(char));
  little = false;
  offset = 0;
  type = 0;
  srate = 0;
  curend = INITIAL_READ_SIZE;
  commenting = false;
  while (happy)
    {
      for (i = 0; i < 256; i++)
	{
	  if (k == curend)
	    {
	      offset += curend;
	      if (read(chan, hdrbuf, HDRBUFSIZ) != HDRBUFSIZ) 
		{
		  FREE(line);
		  return(mus_error(MUS_HEADER_READ_FAILED, "%s comdisco header truncated?", filename));
		}
	      k = 0;
	      curend = HDRBUFSIZ;
	    }
	  if (hdrbuf[k] == '\n') 
	    {
	      k++; 
	      break;
	    }
	  line[i] = hdrbuf[k++];
	}
      line[i] = '\0';
      if ((strcmp(line, "$DATA BINARY") == 0) || 
	  (strcmp(line, "$DATA ASCII") == 0)) 
	{
	  happy = false; 
	  data_location = offset + k;
	}
      if (strcmp(line, "$USER_COMMENT") == 0)
	{
	  comment_start = offset + k;
	  commenting = true;
	}
      else
	{
	  if (commenting)
	    {
	      if (line[0] == '$')
		{
		  comment_end = offset + k - 2 - strlen(line);
		  commenting = false;
		}
	    }
	}
      if (line[0] != '$')
	{
	  char portion[32];
	  char value[32];
	  len = strlen(line);
	  for (j = 0; j < 8; j++) 
	    portion[j] = line[j];
	  portion[8] ='\0';
	  for (j = 8; j < len; j++) 
	    if (line[j] == '=') 
	      break;
	  for (n = 0, m = j + 2; m < len; m++, n++) 
	    value[n] = line[m];
	  value[n] ='\0';
	  if (strcmp(portion, "Sampling") == 0) sscanf(value, "%d", &srate); else
	  if (strcmp(portion, "Number o") == 0) sscanf(value, "%d", &d_size); else
	  if (strcmp(portion, "Signal T") == 0) {if (value[1] == 'o') type = 2; else if (value[1] == 'l') type = 1;} else
	  if (strcmp(portion, "Fixed Po") == 0) {if (value[1] == '8') type = 3;}
	}
    }
  /* now clean up this mess */
  if (data_location == 0)
    return(mus_error(MUS_HEADER_READ_FAILED, "%s: no $DATA BINARY field?", filename));
  if (srate == 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: srate == 0", filename));
  chans = 1;
  if (d_size != 0) data_size = (off_t)d_size;
  switch (type)
    {
    case 0: if (little) data_format = MUS_LSHORT; else data_format = MUS_BSHORT; break;
    case 1: if (little) data_format = MUS_LFLOAT; else data_format = MUS_BFLOAT; break;
    case 2: if (little) data_format = MUS_LDOUBLE; else data_format = MUS_BDOUBLE; break;
    case 3: data_format = MUS_BYTE; break;
    }
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > mus_bytes_to_samples(data_format, true_file_length - data_location))
    data_size = mus_bytes_to_samples(data_format, true_file_length - data_location);
  FREE(line);
  return(MUS_NO_ERROR);
}


/* ------------------------------------ MS ASF -------------------------------------
 *
 * asf format is described at http://www.microsoft.com/asf/specs.htm
 * http://www.microsoft.com/asf/spec3/ASF0198ps.exe
 *
 * this header is completely insane
 */

static int read_asf_header(const char *filename, int chan)
{
  /* a chunked data format, so not really acceptable here or elsewhere -- needs to be unchunked */
  int len, ilen = 0, i, j, bits = 0;
  bool asf_huge = false, present;
  /* apparently "huge" has some meaning in Windoze C */
  len = mus_char_to_lint((unsigned char *)(hdrbuf + 16)); /* actually 64 bits */
  i = (128+64) / 8;
  srate = 0;
  chans = 0;
  while (i < len)
    {
      seek_and_read(chan, (unsigned char *)hdrbuf, i, HDRBUFSIZ);
      if ((unsigned int)(hdrbuf[1]) == 0x29) 
	switch (hdrbuf[0])
	  {
	  case 0xd0: 
	    asf_huge = (hdrbuf[((128+64+128+64+64+64+64+32)/8)] & 2);
	    break;
	  case 0xd4: 
	    present = ((hdrbuf[16+8+16+8+8+ 4+4+4+4+ 4+4] >> 3) & 0x3);
	    if (present)
	      j = 16+8+16+8+8+ 4+4+4+4+ 4+4+ 4+ (4+4+4) + 2;
	    else j = 16+8+16+8+8+ 4+4+4+4+ 4+4+ 4+ 2;
	    srate = mus_char_to_lint((unsigned char *)(hdrbuf + j+11+36));
	    bits = mus_char_to_lint((unsigned char *)(hdrbuf + j+11+32));
	    chans = mus_char_to_ulshort((unsigned char *)(hdrbuf + j+65));
	    original_data_format = mus_char_to_lint((unsigned char *)(hdrbuf + j+11));
	    break;
	  default: break;
	  }
      ilen = mus_char_to_lint((unsigned char *)(hdrbuf + 16));
      if (ilen <= 0) break;
      if ((chans > 0) && (srate > 0)) break;
      i += ilen;
    }
  i = len;
  seek_and_read(chan, (unsigned char *)hdrbuf, i, HDRBUFSIZ);
  data_format = MUS_UNKNOWN;
  if (((unsigned int)(hdrbuf[1]) == 0x29) && ((unsigned int)(hdrbuf[0]) == 0xd2))
    {
      int a_huge = 2;
      ilen = mus_char_to_lint((unsigned char *)(hdrbuf + 16));
      if (asf_huge) a_huge = 4;
      data_location = i + 20 + a_huge + 2+4+3+1;
      if (bits == 0) bits = 8; 
      data_format = wave_to_sndlib_format(original_data_format, bits, true);
    }
  else return(mus_error(MUS_HEADER_READ_FAILED, "%s: unknown data format", filename));
  data_size = ilen - data_location;
  true_file_length = SEEK_FILE_LENGTH(chan);
  if (data_size > true_file_length)
    {
      data_size = true_file_length - data_location;
      if (data_size < 0) return(mus_error(MUS_HEADER_READ_FAILED, "%s: data_size = " OFF_TD "?", filename, data_size));
    }
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}



/* ------------------------------------ no header ------------------------------------- */

static int header_raw_srate = 44100;
static int header_raw_chans = 2;
static int header_raw_format = MUS_BSHORT;

static int read_no_header(const char *filename, int chan)
{
  srate = header_raw_srate;
  chans = header_raw_chans;
  data_format = header_raw_format;
  data_location = 0;
  data_size = SEEK_FILE_LENGTH(chan);
  true_file_length = data_size;
  data_size = mus_bytes_to_samples(data_format, data_size);
  return(MUS_NO_ERROR);
}

void mus_header_set_raw_defaults(int sr, int chn, int frm)
{
  if (sr > 0) header_raw_srate = sr;
  if (chn > 0) header_raw_chans = chn;
  if (MUS_DATA_FORMAT_OK(frm)) header_raw_format = frm;
}

void mus_header_raw_defaults(int *sr, int *chn, int *frm)
{
  (*sr) = header_raw_srate;
  (*chn) = header_raw_chans;
  (*frm) = header_raw_format;
}


/* ------------------------------------ all together now ------------------------------------ */

static int mus_header_read_1(const char *filename, int chan)
{
  /* returns 0 on success (at least to the extent that we can report the header type), -1 for error */
  int i, loc = 0, bytes;
  bool happy;
  header_type = MUS_UNSUPPORTED;
  data_format = MUS_UNKNOWN;
  comment_start = 0;
  comment_end = 0;
  data_size = 0;
  data_location = 0;
  if (loop_modes)
    {
      loop_modes[0] = 0;
      loop_modes[1] = 0;
    }
  bytes = read(chan, hdrbuf, INITIAL_READ_SIZE);
  /* if it's a 0 length file we need to get out */
  if (bytes < 0) 
    return(mus_error(MUS_HEADER_READ_FAILED,	"%s: %s", filename, (errno) ? STRERROR(errno) : "bytes read < 0?"));
  if (bytes == 0) 
    {
      header_type = MUS_RAW;
      srate = header_raw_srate;
      chans = header_raw_chans;
      data_format = header_raw_format;
      data_location = 0;
      true_file_length = 0;
      return(MUS_NO_ERROR);
    }
  if (bytes < 4) 
    {
      header_type = MUS_RAW;
      return(read_no_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_DSND)) || 
      (match_four_chars((unsigned char *)hdrbuf, I_DECN)))
    {
      if (bytes < 24) 
	return(mus_error(MUS_HEADER_READ_FAILED, "%s NeXT header truncated? found only %d bytes", filename, bytes));
      header_type = MUS_NEXT;
      return(read_next_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_FORM))
    {
      /* next 4 bytes are apparently the file size or something equally useless */
      if (bytes < 12) 
	return(mus_error(MUS_HEADER_READ_FAILED, "%s AIFF header truncated? found only %d bytes", filename, bytes));
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_AIFF))
	{ 
	  header_type = MUS_AIFF;
	  return(read_aiff_header(filename, chan, 0));
	}
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_AIFC))
	{ 
	  header_type = MUS_AIFC;
	  return(read_aiff_header(filename, chan, 0));
	}
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_8SVX))
	{
	  header_type = MUS_SVX;
	  return(read_8svx_header(filename, chan, true));
	}
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_16SV))
	{
	  header_type = MUS_SVX;
	  return(read_8svx_header(filename, chan, false));
	}
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_MAUD))
	{
	  header_type = MUS_MAUD;
	  return(read_maud_header(filename, chan));
	}
      /* apparently SAMP here -> sampled audio data (?) */

      if (match_four_chars((unsigned char *)(hdrbuf + 4), I_DS16))
	{
	  header_type = MUS_CSL;
	  return(read_csl_header(filename, chan));
	}
      return(mus_error(MUS_HEADER_READ_FAILED,	"%s: unrecognized \"FORM\" (i.e. AIFF) header",	filename));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_RIFF)) ||
      (match_four_chars((unsigned char *)hdrbuf, I_RIFX)))
    {
      if (bytes < 12) 
	return(mus_error(MUS_HEADER_READ_FAILED, "%s RIFF header truncated? found only %d bytes", filename, bytes));
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_WAVE))
	{
	  header_type = MUS_RIFF;
	  return(read_riff_header(filename, chan));
	}
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_sfbk))
	{
	  header_type = MUS_SOUNDFONT;
	  return(read_soundfont_header(filename, chan));
	}
      if (match_four_chars((unsigned char *)(hdrbuf + 8), I_AVI_))
	{
	  header_type = MUS_AVI;
	  return(read_avi_header(filename, chan));
	}
      return(mus_error(MUS_HEADER_READ_FAILED, "%s: unrecognized \"RIFF\" (i.e. 'wave') header", filename));
    }
  if ((equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_VAX)) || 
      (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_SUN)) ||
      (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_MIPS)) || 
      (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_NEXT)))
    {
      if (bytes < 24) 
	return(mus_error(MUS_HEADER_READ_FAILED, "%s IRCAM header truncated? found only %d bytes", filename, bytes));
      header_type = MUS_IRCAM;
      return(read_ircam_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_NIST))
    {
      header_type = MUS_NIST;
      return(read_nist_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SOUN))
    {
      if ((match_four_chars((unsigned char *)(hdrbuf + 4), I_D_SA)) && 
	  (match_four_chars((unsigned char *)(hdrbuf + 8), I_MPLE)))
	{
	  header_type = MUS_SMP;
	  return(read_smp_header(filename, chan));
	}
      else
	{
	  header_type = MUS_SNDT;
	  return(read_sndt_header(filename, chan));
	}
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_VOC0)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_VOC1)))
    {
      if (bytes < 24) 
	return(mus_error(MUS_HEADER_READ_FAILED, "%s VOC header truncated? found only %d bytes", filename, bytes));
      header_type = MUS_VOC;
      return(read_voc_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_AVR_))
    {
      header_type = MUS_AVR;
      return(read_avr_header(filename, chan));
    }
  if (mus_char_to_bshort((unsigned char *)hdrbuf) == 1336)
    {
      header_type = MUS_SD1;
      return(read_sd1_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_ALaw)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_Soun)))
    {
      header_type = MUS_PSION;
      return(read_psion_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_GF1P)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_ATCH)))
    {
      header_type = MUS_GRAVIS;
      return(read_gravis_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_DSIG)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_NAL_)))
    {
      header_type = MUS_COMDISCO;
      return(read_comdisco_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_GOLD)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I__WAV)))
    {
      header_type = MUS_GOLDWAVE;
      return(read_goldwave_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_Diam)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_ondW)))
    {
      header_type = MUS_DIAMONDWARE;
      return(read_diamondware_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SRFS))
    {
      header_type = MUS_SRFS;
      return(read_srfs_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_CSRE))
    {
      header_type = MUS_ADF;
      return(read_adf_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_fLaC))
    {
      header_type = MUS_FLAC;
      return(MUS_NO_ERROR);
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_OggS))
    {
      if ((hdrbuf[29] == 'v') && (hdrbuf[30] == 'o') && (hdrbuf[31] == 'r'))
	header_type = MUS_OGG;
      else
	{
	  if ((hdrbuf[28] == 'S') && (hdrbuf[29] == 'p') && (hdrbuf[30] == 'e'))
	    header_type = MUS_SPEEX;
	}
      return(MUS_NO_ERROR);
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_file)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 4), I__sam)))
    {
      header_type = MUS_FILE_SAMP;
      return(read_file_samp_header(filename, chan));
    }
  if ((hdrbuf[0] == 0xf0) && (hdrbuf[1] == 0x7e) && (hdrbuf[3] == 0x01))
    {
      header_type = MUS_MIDI_SAMPLE_DUMP;
      chans = 1;
      srate = (int)(1.0e9 / (float)((hdrbuf[7] + (hdrbuf[8] << 7) + (hdrbuf[9] << 14))));
      data_size = (hdrbuf[10] + (hdrbuf[11] << 7) + (hdrbuf[12] << 14));
      /* since this file type has embedded blocks, we have to translate it elsewhere */
      return(MUS_NO_ERROR);
    }
  /* no recognized magic number at start -- poke around in possible header for other types */
  /* ESPS is either 0x00006a1a or 0x1a6a0000 at byte 16 */
  if (equal_big_or_little_endian((unsigned char *)(hdrbuf + 16), 0x00006a1a))
    {
      header_type = MUS_ESPS;
      return(read_esps_header(filename, chan));
    }
  lseek(chan, 0, SEEK_SET);
  CHK_READ(chan, hdrbuf, 256);
  if ((hdrbuf[252] == 64) && (hdrbuf[253] == 195)) /* #o100 and #o303 */
    {
      header_type = MUS_SPPACK;
      return(read_sppack_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)(hdrbuf + 65), I_FSSD)) && 
      (match_four_chars((unsigned char *)(hdrbuf + 128), I_HCOM)))
    {
      header_type = MUS_HCOM;
      return(MUS_NO_ERROR);
    }
  happy = false;
  for (i = 0; i < NINRS; i++) 
    {
      if (equal_big_or_little_endian((unsigned char *)hdrbuf, I_INRS[i]))
	{
	  happy = true;
	  loc = inrs_srates[i];
	}
    }
  if (happy)
    {
      header_type = MUS_INRS;
      return(read_inrs_header(filename, chan, loc));
    }
  if (mus_char_to_ubint((unsigned char *)hdrbuf) == 0xAAAAAAAA)
    {
      header_type = MUS_MUS10;
      return(MUS_NO_ERROR);
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_SPIB)) || 
      (match_four_chars((unsigned char *)hdrbuf, I_S___)))
    {
      header_type = MUS_IEEE;
      return(MUS_NO_ERROR);
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_riff))
    {
      header_type = MUS_SOUNDFORGE;
      return(read_soundforge_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I_PVF1)) || 
      (match_four_chars((unsigned char *)hdrbuf, I_PVF2)))
    {
      header_type = MUS_PVF;
      return(read_pvf_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_MThd))
    {
      header_type = MUS_MIDI;
      return(MUS_ERROR);
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SND_))
    {
      header_type = MUS_SBSTUDIOII;
      return(read_sbstudio_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_FSMt))
    {
      header_type = MUS_FARANDOLE;
      return(read_farandole_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SDXc))
    {
      header_type = MUS_SAMPLE_DUMP;
      return(read_sample_dump_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_DDSF))
    {
      header_type = MUS_DELUSION;
      return(read_delusion_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_LM89))
    {
      header_type = MUS_YAMAHA_TX16W;
      return(read_tx16w_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SY85))
    {
      header_type = MUS_YAMAHA_SY85;
      return(read_sy85_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SY80))
    {
      header_type = MUS_YAMAHA_SY99;
      return(read_sy85_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_PRAM))
    {
      header_type = MUS_KURZWEIL_2000;
      return(read_kurzweil_2000_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SMP1))
    {
      header_type = MUS_KORG;
      return(read_korg_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_Maui))
    {
      header_type = MUS_MAUI;
      return(read_maui_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_IMPS))
    {
      header_type = MUS_IMPULSETRACKER;
      return(read_impulsetracker_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)(hdrbuf + 35), I_UWFD))
    {
      header_type = MUS_ULTRATRACKER;
      return(read_ultratracker_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)(hdrbuf + 76), I_SCRS))
    {
      header_type = MUS_DIGIPLAYER;
      return(read_digiplayer_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_covox))
    {
      header_type = MUS_COVOX;
      return(read_covox_header(filename, chan));
    }
  if ((match_four_chars((unsigned char *)hdrbuf, I__PAF)) ||
      (match_four_chars((unsigned char *)hdrbuf, I_FAP_)))
    {
      header_type = MUS_PAF;
      return(read_paf_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_TWIN))
    {
      header_type = MUS_TWINVQ;
      return(read_twinvq_header(filename, chan));
    }
#if MUS_LITTLE_ENDIAN
  if (mus_char_to_uninterpreted_int((unsigned char *)hdrbuf) == 0x01000800)
#else
  if (mus_char_to_uninterpreted_int((unsigned char *)hdrbuf) == 0x00080001)
#endif
    {
      header_type = MUS_ADC;
      return(read_adc_header(filename, chan));
    }

  if ((match_four_chars((unsigned char *)hdrbuf, I_ones)) &&
      (match_four_chars((unsigned char *)(hdrbuf + 12), I_FORM)))
    {
      /* possibly an OMF file with an embedded AIFF data file -- this is just a guess... */
      header_type = MUS_OMF;
      return(read_aiff_header(filename, chan, 12));
      /* another (apparently) along these lines is TOC */
    }

  if ((match_four_chars((unsigned char *)hdrbuf, I_zeros)) &&
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_mdat)))
    {
      /* possibly quicktime?? */
      header_type = MUS_QUICKTIME;
      return(read_qt_header(filename, chan));
    }
  if ((hdrbuf[0] == 1) && (hdrbuf[1] == 4)) /* name follows --check? */
    {
      header_type = MUS_AKAI4;
      return(read_akai4_header(filename, chan));
    }
#if 0
  if ((hdrbuf[0] == 3) && (hdrbuf[16] == 128))
    {
      header_type = MUS_AKAI4;
      return(read_akai3_header(filename, chan));
    }
#endif
  if ((match_four_chars((unsigned char *)hdrbuf, I_asf0)) &&
      (match_four_chars((unsigned char *)(hdrbuf + 4), I_asf1)) &&
      (match_four_chars((unsigned char *)(hdrbuf + 8), I_asf2)) &&
      (match_four_chars((unsigned char *)(hdrbuf + 12), I_asf3)))
    {
      header_type = MUS_ASF;
      return(read_asf_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_SDIF))
    {
      header_type = MUS_SDIF;
      return(read_sdif_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_NVF_))
    {
      header_type = MUS_NVF;
      return(read_nvf_header(filename, chan));
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_ajkg))
    {
      header_type = MUS_SHORTEN;
      return(MUS_NO_ERROR);
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_TTA1))
    {
      header_type = MUS_TTA;
      return(MUS_NO_ERROR);
    }
  if (match_four_chars((unsigned char *)hdrbuf, I_wvpk))
    {
      header_type = MUS_WAVPACK;
      return(MUS_NO_ERROR);
    }

  header_type = MUS_RAW;
  return(read_no_header(filename, chan));
}

static int local_error_type = MUS_NO_ERROR;
static char *local_error_msg = NULL;
static mus_error_handler_t *old_error_handler;

static void local_mus_error(int type, char *msg)
{
  local_error_type = type;
  if (local_error_msg) 
    free(local_error_msg);
  if (msg)
    local_error_msg = strdup(msg);
  else local_error_msg = NULL;
}

int mus_header_read(const char *name)
{
  int chan, err = 0;
  chan = mus_file_open_read(name);
  if (chan == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "can't open %s: %s", name, STRERROR(errno)));
  old_error_handler = mus_error_set_handler(local_mus_error);
  err = mus_header_read_1(name, chan);
  mus_error_set_handler(old_error_handler);
  CLOSE(chan, name);
  if (err != MUS_NO_ERROR)
    return(mus_error(local_error_type, local_error_msg)); /* pass error info on up the chain now that we've cleaned up the open file descriptor */
  return(err);
}

static mus_header_write_hook_t *mus_header_write_hook = NULL;

mus_header_write_hook_t *mus_header_write_set_hook(mus_header_write_hook_t *new_hook) 
{
  mus_header_write_hook_t *old_hook;
  old_hook = mus_header_write_hook;
  mus_header_write_hook = new_hook;
  return(old_hook);
}

int mus_header_write(const char *name, int type, int in_srate, int in_chans, off_t loc, off_t size_in_samples, int format, const char *comment, int len)
{
  /* the "loc" arg is a mistake -- just always set it to 0 */
  int chan, err = MUS_NO_ERROR;
  off_t siz;
  chan = mus_file_create(name);
  if (chan == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "can't write %s: %s", name, STRERROR(errno)));
  if (mus_header_write_hook)
    (*mus_header_write_hook)(name);
  siz = mus_samples_to_bytes(format, size_in_samples);
  /* no mus_error calls within any of the write functions */
  switch (type)
    {
    case MUS_NEXT: err = mus_header_write_next_header(chan, in_srate, in_chans, loc, siz, format, comment, len); break;
    case MUS_AIFC: err = write_aif_header(chan, in_srate, in_chans, siz, format, comment, len, true); break;
    case MUS_AIFF: err = write_aif_header(chan, in_srate, in_chans, siz, format, comment, len, false); break;
    case MUS_RIFF: err = write_riff_header(chan, in_srate, in_chans, siz, format, comment, len); break;
    case MUS_IRCAM: err = write_ircam_header(chan, in_srate, in_chans, format, comment, len); break;
    case MUS_NIST: err = write_nist_header(chan, in_srate, in_chans, siz, format); break;
    case MUS_RAW: 
      data_location = 0; 
      data_size = mus_bytes_to_samples(format, siz);
      srate = in_srate; 
      chans = in_chans; 
      header_type = MUS_RAW;
      data_format = format;
      break;
    default:
      CLOSE(chan, name);
      return(mus_error(MUS_UNSUPPORTED_HEADER_TYPE,  "can't write %s header for %s", mus_header_type_name(type), name));
      break;
    }
  CLOSE(chan, name);
  return(err);
}

int mus_header_change_data_size(const char *filename, int type, off_t size) /* in bytes */
{
  /* the read header at sample update (sound-close) time could be avoided if the
   *   ssnd_location (etc) were saved and passed in -- perhaps an added optimized
   *   header change data size?  Means saving the relevant data, and exporting it
   *  from headers.c. Can we guarantee consistency here?
   */
  int chan, err = MUS_NO_ERROR;
  switch (type)
    {
    case MUS_AIFF:
    case MUS_AIFC:
    case MUS_NIST:
    case MUS_RIFF:
      err = mus_header_read(filename);
      break;
    default:
      break;
    }
  if (err == MUS_ERROR) return(err);
  chan = mus_file_reopen_write(filename);
  if (chan == -1) return(mus_error(MUS_HEADER_WRITE_FAILED, "%s: %s", filename, STRERROR(errno)));
  switch (type)
    {
    case MUS_NEXT: 
      lseek(chan, 8L, SEEK_SET);
      mus_bint_to_char((unsigned char *)(hdrbuf + 0), (int)size);
      CHK_WRITE(chan, hdrbuf, 4);
      break;
    case MUS_AIFC: 
    case MUS_AIFF: 
      /* we apparently have to make sure the form size and the data size are correct 
       * assumed here that we'll only be updating our own AIFF files 
       * There are 3 such locations -- the 2nd word of the file which is the overall form size, 
       * the frames variable in the COMM chunk, and the chunk-size variable in the SSND chunk 
       * an unexpected hassle for CLM is that we can open/close the output file many times if running mix,
       * so we have to update the various size fields taking into account the old size 
       */

      /* read sets current update_form_size, data_size, data_format, update_frames_location, update_ssnd_location */

      lseek(chan, 4L, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, (int)size + update_form_size - mus_samples_to_bytes(data_format, data_size));
      /* cancel old data_size from previous possible write */
      CHK_WRITE(chan, hdrbuf, 4);
      lseek(chan, update_frames_location, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, (int)size / (chans * mus_bytes_per_sample(data_format)));
      CHK_WRITE(chan, hdrbuf, 4);
      lseek(chan, update_ssnd_location, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, (int)size + 8);
      CHK_WRITE(chan, hdrbuf, 4);
      break;
    case MUS_RIFF: 

      /* read sets current update_form_size, data_format, data_size, update_ssnd_location */

      lseek(chan, 4L, SEEK_SET);
      mus_lint_to_char((unsigned char *)hdrbuf, (int)size + update_form_size - mus_samples_to_bytes(data_format, data_size)); 
      CHK_WRITE(chan, hdrbuf, 4);
      lseek(chan, update_ssnd_location, SEEK_SET);
      mus_lint_to_char((unsigned char *)hdrbuf, (int)size);
      CHK_WRITE(chan, hdrbuf, 4);
      break;
    case MUS_IRCAM: 
    case MUS_RAW:
      /* size is implicit in file size */
      break;
    case MUS_NIST: 

      /* read sets current srate, chans, data_format */

      lseek(chan, 0L, SEEK_SET);
      write_nist_header(chan, mus_header_srate(), mus_header_chans(), size, mus_header_format());
      break;
    default:
      CLOSE(chan, filename);
      return(mus_error(MUS_UNSUPPORTED_HEADER_TYPE, "mus_header_change_data_size: can't update %s headers", mus_header_type_name(type)));
      break;
    }
  CLOSE(chan, filename);
  return(err);
}

int mus_header_change_chans(const char *filename, int type, int new_chans)
{
  int err = MUS_NO_ERROR, fd;
  off_t new_frames;
  switch (type)
    {
    case MUS_AIFF:
    case MUS_AIFC:
    case MUS_NIST:
    case MUS_RIFF:
      err = mus_header_read(filename);
      break;
    default:
      break;
    }
  if (err == MUS_ERROR) return(err);
  fd = mus_file_reopen_write(filename);
  if (fd == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "mus_header_change_chans for %s failed: %s", filename, STRERROR(errno)));
  switch (type)
    {
    case MUS_NEXT:
      lseek(fd, 20L, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, new_chans);
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    case MUS_IRCAM:
      lseek(fd, 8L, SEEK_SET);
      if (little_endian) 
	mus_lint_to_char((unsigned char *)hdrbuf, new_chans);
      else mus_bint_to_char((unsigned char *)hdrbuf, new_chans);
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    case MUS_NIST:
      lseek(fd, 0L, SEEK_SET);
      write_nist_header(fd, srate, new_chans, mus_bytes_per_sample(data_format) * data_size, data_format);
      /* header size is always 1024, so this is safe */
      break;
    case MUS_AIFF: case MUS_AIFC:
      lseek(fd, update_frames_location - 2, SEEK_SET);
      new_frames = data_size / new_chans;
      mus_bshort_to_char((unsigned char *)hdrbuf, new_chans);
      mus_bint_to_char((unsigned char *)(hdrbuf + 2), new_frames);
      CHK_WRITE(fd, hdrbuf, 6);
      break;
    case MUS_RIFF:
      lseek(fd, update_frames_location - 2, SEEK_SET);
      if (little_endian)
	mus_lshort_to_char((unsigned char *)hdrbuf, new_chans);
      else mus_bshort_to_char((unsigned char *)hdrbuf, new_chans);
      CHK_WRITE(fd, hdrbuf, 2);
      break;
    }
  CLOSE(fd, filename);
  return(err);
}

int mus_header_change_srate(const char *filename, int type, int new_srate)
{
  int err = MUS_NO_ERROR, fd;
  switch (type)
    {
    case MUS_AIFF:
    case MUS_AIFC:
    case MUS_NIST:
    case MUS_RIFF:
      err = mus_header_read(filename);
      break;
    default:
      break;
    }
  if (err == MUS_ERROR) return(err);
  fd = mus_file_reopen_write(filename);
  if (fd == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "mus_header_change_srate for %s failed: %s", filename, STRERROR(errno)));
  switch (type)
    {
    case MUS_NEXT:
      lseek(fd, 16L, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, new_srate);
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    case MUS_IRCAM:
      lseek(fd, 4L, SEEK_SET);
      if (little_endian) 
	mus_lfloat_to_char((unsigned char *)hdrbuf, (float)new_srate);
      else mus_bfloat_to_char((unsigned char *)hdrbuf, (float)new_srate);
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    case MUS_NIST:
      lseek(fd, 0L, SEEK_SET);
      write_nist_header(fd, new_srate, chans, mus_bytes_per_sample(data_format) * data_size, data_format);
      break;
    case MUS_AIFF: 
    case MUS_AIFC:
      lseek(fd, update_frames_location + 6, SEEK_SET);
      double_to_ieee_80((double)new_srate, (unsigned char *)hdrbuf);
      CHK_WRITE(fd, hdrbuf, 10);
      break;
    case MUS_RIFF:
      lseek(fd, update_frames_location, SEEK_SET);
      if (little_endian)
	mus_lint_to_char((unsigned char *)hdrbuf, new_srate);
      else mus_bint_to_char((unsigned char *)hdrbuf, new_srate);
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    }
  CLOSE(fd, filename);
  return(err);
}

int mus_header_change_type(const char *filename, int new_type, int new_format)
{
  int err = MUS_NO_ERROR;
  /* open temp, write header, copy data, replace original with temp */
  err = mus_header_read(filename);
  if (err == MUS_NO_ERROR)
    {
      if (header_type != new_type)
	{
	  int ofd, ifd, nbytes;
	  off_t loc, len = 0;
	  char *buf = NULL;
	  char *new_file, *comment = NULL;
	  new_file = (char *)CALLOC(strlen(filename) + 5, sizeof(char));
	  sprintf(new_file, "%s.tmp", filename);
	  loc = mus_header_data_location();
	  if (new_type != MUS_RAW)
	    {
	      if (comment_end > comment_start)
		{
		  len = comment_end - comment_start + 1;
		  comment = (char *)CALLOC(len + 1, sizeof(char));
		  ifd = mus_file_open_read(filename);
		  lseek(ifd, comment_start, SEEK_SET);
		  CHK_READ(ifd, (unsigned char *)comment, len);
		  CLOSE(ifd, filename);
		}
	      data_size = data_size * mus_bytes_per_sample(data_format) / mus_bytes_per_sample(new_format);
	      mus_header_write(new_file, new_type, srate, chans, loc, data_size, new_format, comment, len);
	    }
	  else mus_file_create(new_file);
	  ifd = mus_file_open_read(filename);
	  lseek(ifd, loc, SEEK_SET);
	  ofd = mus_file_reopen_write(new_file);
	  lseek(ofd, 0L, SEEK_END);
	  buf = (char *)CALLOC(8192, sizeof(char));
	  while ((nbytes = read(ifd, buf, 8192))) CHK_WRITE(ofd, buf, nbytes);
	  CLOSE(ifd, filename);
	  CLOSE(ofd, new_file);
	  FREE(buf);
	  if (comment) FREE(comment);
	  rename(new_file, filename);
	  FREE(new_file);
	}
    }
  return(err);
}

int mus_header_change_format(const char *filename, int type, int new_format)
{
  int err = MUS_NO_ERROR, fd;
  off_t old_bytes;
  switch (type)
    {
    case MUS_AIFF:
    case MUS_AIFC:
    case MUS_NIST:
    case MUS_RIFF:
      err = mus_header_read(filename);
      break;
    default:
      break;
    }
  if (err == MUS_ERROR) return(err);
  fd = mus_file_reopen_write(filename);
  if (fd == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "mus_header_change_format for %s failed: %s", filename, STRERROR(errno)));
  switch (type)
    {
    case MUS_NEXT:
      lseek(fd, 12L, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, sndlib_format_to_next(new_format));
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    case MUS_IRCAM:
      lseek(fd, 12L, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, sndlib_format_to_ircam(new_format));
      CHK_WRITE(fd, hdrbuf, 4);
      break;
    case MUS_NIST:
      lseek(fd, 0L, SEEK_SET);
      write_nist_header(fd, srate, chans, mus_bytes_per_sample(data_format) * data_size, new_format);
      break;
    case MUS_AIFF: 
    case MUS_AIFC:
      old_bytes = data_size * mus_bytes_per_sample(data_format);
      lseek(fd, update_frames_location, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, old_bytes / (chans * mus_bytes_per_sample(new_format)));
      mus_bshort_to_char((unsigned char *)(hdrbuf + 4), sndlib_format_to_aiff_bits(new_format));
      CHK_WRITE(fd, hdrbuf, 6);
      if (header_type == MUS_AIFC)
	{
	  const char *str;
	  str = sndlib_format_to_aifc_name(new_format);
	  lseek(fd, update_frames_location + 16, SEEK_SET);
	  write_four_chars((unsigned char *)(hdrbuf + 0), (const unsigned char *)str);
	  (*(unsigned char *)(hdrbuf + 4)) = 4; /* final pad null not accounted-for */
	  write_four_chars((unsigned char *)(hdrbuf + 5), (const unsigned char *)str);
	  (*(unsigned char *)(hdrbuf + 9)) = 0;
	  CHK_WRITE(fd, hdrbuf, 10);
	}
      break;
    case MUS_RIFF:
      lseek(fd, update_frames_location + 24, SEEK_SET);
      if (little_endian)
	mus_lshort_to_char((unsigned char *)hdrbuf, sndlib_format_to_aiff_bits(new_format));
      else mus_bshort_to_char((unsigned char *)hdrbuf, sndlib_format_to_aiff_bits(new_format));
      CHK_WRITE(fd, hdrbuf, 2);
      lseek(fd, update_frames_location + 10, SEEK_SET);
      switch (new_format)
	{
	case MUS_MULAW: 
	  new_format = 7; 
	  break;
	case MUS_ALAW:  
	  new_format = 6; 
	  break;
	case MUS_UBYTE: 
	case MUS_LSHORT: case MUS_L24INT: case MUS_LINT: 
	case MUS_BSHORT: case MUS_B24INT: case MUS_BINT: 
	  new_format = 1;
	  break;
	case MUS_LFLOAT: case MUS_LDOUBLE: 
	case MUS_BFLOAT: case MUS_BDOUBLE:
	  new_format = 3;
	  break;
	}
      if (little_endian)
	mus_lshort_to_char((unsigned char *)hdrbuf, new_format);
      else mus_bshort_to_char((unsigned char *)hdrbuf, new_format);
      CHK_WRITE(fd, hdrbuf, 2);
      break;
    }
  CLOSE(fd, filename);
  return(err);
}

int mus_header_change_location(const char *filename, int type, off_t new_location)
{
  /* only Next/Sun changeable in this regard */
  int err = MUS_NO_ERROR, fd;
  fd = mus_file_reopen_write(filename);
  if (fd == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "mus_header_change_location for %s failed: %s", filename, STRERROR(errno)));
  if (type == MUS_NEXT)
    {
      lseek(fd, 4L, SEEK_SET);
      mus_bint_to_char((unsigned char *)hdrbuf, new_location);
      CHK_WRITE(fd, hdrbuf, 4);
    }
  CLOSE(fd, filename);
  return(err);
}

int mus_header_change_comment(const char *filename, int type, char *new_comment)
{
  int err = MUS_NO_ERROR, fd, len = 0;
  bool need_ripple = false;
  err = mus_header_read(filename);
  if (err == MUS_NO_ERROR)
    {
      switch (type)	  
	{
	case MUS_IRCAM:
	  fd = mus_file_reopen_write(filename);
	  lseek(fd, 16L, SEEK_SET);
	  if (new_comment) len = strlen(new_comment);
	  write_ircam_comment(fd, new_comment, len);
	  CLOSE(fd, filename);
	  break;
	case MUS_NEXT:
	  fd = mus_file_reopen_write(filename);
	  lseek(fd, 24L, SEEK_SET);
	  if (new_comment == NULL)
	    write_next_comment(fd, new_comment, 0, data_location); /* erase old possibly */
	  else
	    {
	      if ((comment_start != comment_end) && 
		  ((int)(data_location - 24) >= (int)strlen(new_comment)))
		write_next_comment(fd, new_comment, strlen(new_comment), data_location); /* there's room to overwrite old comment */
	      else need_ripple = true;
	    }
	  CLOSE(fd, filename);
	  break;
	default:
	  need_ripple = true;
	  break;
	}
      if (need_ripple)
	{
	  /* open temp, write header, copy data, replace original with temp */
	  char *new_file;
	  int ofd, ifd;
	  off_t loc;
	  int nbytes;
	  char *buf = NULL;
	  new_file = (char *)CALLOC(strlen(filename) + 5, sizeof(char));
	  sprintf(new_file, "%s.tmp", filename);
	  loc = mus_header_data_location();
	  mus_header_write(new_file, header_type, srate, chans, loc, data_size, data_format, new_comment, (new_comment) ? strlen(new_comment) : 0);
	  ifd = mus_file_open_read(filename);
	  lseek(ifd, loc, SEEK_SET);
	  ofd = mus_file_reopen_write(new_file);
	  lseek(ofd, 0L, SEEK_END);
	  buf = (char *)CALLOC(8192, sizeof(char));
	  while ((nbytes = read(ifd, buf, 8192))) CHK_WRITE(ofd, buf, nbytes);
	  CLOSE(ifd, filename);
	  CLOSE(ofd, new_file);
	  FREE(buf);
	  rename(new_file, filename);
	  FREE(new_file);
	}
    }
  return(err);
}

bool mus_header_writable(int type, int format) /* -2 to ignore format for this call */
{
  switch (type)
    {
    case MUS_NEXT:
      if (format == MUS_UNKNOWN) return(false);
      return(true);
      break;
    case MUS_NIST:
      if (format == -2) return(true);
      switch (format)
	{
	case MUS_BYTE: case MUS_BSHORT: case MUS_B24INT: case MUS_BINT: 
	case MUS_LSHORT: case MUS_L24INT: case MUS_LINT: 
	  return(true); break;
	default: 
	  return(false); break;
	}
      break;
    case MUS_AIFC: 
      if (format == -2) return(true);
      switch (format)
	{
	case MUS_BSHORT: case MUS_B24INT: case MUS_BINT:
	case MUS_BYTE: case MUS_MULAW: case MUS_ALAW:
	case MUS_BFLOAT: case MUS_BDOUBLE: case MUS_UBYTE: case MUS_UBSHORT:
	case MUS_LSHORT: case MUS_L24INT: case MUS_LINT:
	  return(true);
	  break;
	default:
	  return(false);
	  break;
	}
      break;
    case MUS_AIFF:
      if (format == -2) return(true);
      switch (format)
	{
	case MUS_BSHORT: case MUS_B24INT: case MUS_BINT: case MUS_BYTE: 
	  return(true);
	  break;
	default: 
	  return(false);
	  break;
	}
      break;
    case MUS_RIFF:
      if (format == -2) return(true);
      switch (format)
	{
	case MUS_MULAW: case MUS_ALAW: case MUS_UBYTE: case MUS_LFLOAT:
	case MUS_LSHORT: case MUS_L24INT: case MUS_LINT: case MUS_LDOUBLE:
	  return(true);
	  break;
	default: 
	  return(false);
	  break;
	}
      break;
    case MUS_IRCAM:
      if (format == -2) return(true);
      switch (format)
	{
	case MUS_MULAW: case MUS_ALAW: case MUS_BSHORT: case MUS_BINT: case MUS_BFLOAT:
	  return(true);
	  break;
	default:
	  return(false);
	  break;
	}
      break;
    case MUS_RAW:
      return(true);
      break;
    default: return(false); break;
    }
  return(false);
}

static char aifc_format[5];

/* try to give some info on data formats that aren't supported by sndlib */
const char *mus_header_original_format_name(int format, int type)
{
  switch (type)
    {
    case MUS_NEXT:
      switch (format)
	{
	case 0: return("unspecified"); break; case 8: return("indirect"); break; case 9: return("nested"); break;
	case 10: return("dsp_core"); break; case 11: return("dsp_data_8"); break; case 12: return("dsp_data_16"); break;
	case 13: return("dsp_data_24"); break; case 14: return("dsp_data_32"); break; case 16: return("display"); break;
	case 17: return("mulaw_squelch"); break; case 18: return("emphasized"); break; case 19: return("compressed"); break;
	case 20: return("compressed_emphasized"); break; case 21: return("dsp_commands"); break; case 22: return("dsp_commands_samples"); break;
	case 23: return("adpcm_g721"); break; case 24: return("adpcm_g722"); break; case 25: return("adpcm_g723"); break;
	case 26: return("adpcm_g723_5"); break; case 28: return("aes"); break; case 29: return("delat_mulaw_8"); break;
	}
      break;
    case MUS_AIFC:
      aifc_format[4] = 0;
#if MUS_LITTLE_ENDIAN
      sprintf(aifc_format, "%c%c%c%c", format & 0xff, (format >> 8) & 0xff, (format >> 16) & 0xff, (format >> 24) & 0xff);
#else
      sprintf(aifc_format, "%c%c%c%c", (format >> 24) & 0xff, (format >> 16) & 0xff, (format >> 8) & 0xff, format & 0xff);
#endif	
      return(aifc_format);
      break;
    case MUS_PVF:
      if (type_specifier == mus_char_to_uninterpreted_int((unsigned const char *)I_PVF2))
	return("ascii text");
      break;
    case MUS_RIFF:
      switch (format)
	{
	case 2: return("ADPCM"); break; case 4: return("VSELP"); break; case 5: return("IBM_CVSD"); break;
	case 0x10: return("OKI_ADPCM"); break; case 0x11: return("DVI_ADPCM"); break; case 0x12: return("MediaSpace_ADPCM"); break;
	case 0x13: return("Sierra_ADPCM"); break; case 0x14: return("G723_ADPCM"); break; case 0x15: return("DIGISTD"); break;
	case 0x16: return("DIGIFIX"); break; case 0x17: return("Dialogic ADPCM"); break; case 0x18: return("Mediavision ADPCM"); break;
	case 0x19: return("HP cu codec"); break; case 0x20: return("Yamaha_ADPCM"); break; case 0x21: return("SONARC"); break;
	case 0x22: return("DSPGroup_TrueSpeech"); break; case 0x23: return("EchoSC1"); break; case 0x24: return("AudioFile_AF36"); break;
	case 0x25: return("APTX"); break; case 0x26: return("AudioFile_AF10"); break; case 0x27: return("prosody 1612"); break;
	case 0x28: return("lrc"); break; case 0x30: return("Dolby_Ac2"); break; case 0x31: return("GSM610"); break;
	case 0x32: return("MSN audio codec"); break; case 0x33: return("Antext_ADPCM"); break; case 0x34: return("Control_res_vqlpc"); break;
	case 0x35: return("DIGIREAL"); break; case 0x36: return("DIGIADPCM"); break; case 0x37: return("Control_res_cr10"); break;
	case 0x38: return("NMS_VBXADPCM"); break; case 0x39: return("oland rdac"); break; case 0x3a: return("echo sc3"); break;
	case 0x3b: return("Rockwell adpcm"); break; case 0x3c: return("Rockwell digitalk codec"); break; case 0x3d: return("Xebec"); break;
	case 0x40: return("G721_ADPCM"); break; case 0x41: return("G728 CELP"); break; case 0x42: return("MS G723"); break;
	case 0x50: return("MPEG"); break; case 0x52: return("RT24"); break; case 0x53: return("PAC"); break;
	case 0x55: return("Mpeg layer 3"); break; case 0x59: return("Lucent G723"); break; case 0x60: return("Cirrus"); break;
	case 0x61: return("ESS Tech pcm"); break; case 0x62: return("voxware "); break; case 0x63: return("canopus atrac"); break;
	case 0x64: return("G726"); break; case 0x65: return("G722"); break; case 0x66: return("DSAT"); break;
	case 0x67: return("DSAT display"); break; case 0x69: return("voxware "); break; case 0x70: return("voxware ac8 "); break;
	case 0x71: return("voxware ac10 "); break; case 0x72: return("voxware ac16"); break; case 0x73: return("voxware ac20"); break;
	case 0x74: return("voxware rt24"); break; case 0x75: return("voxware rt29"); break; case 0x76: return("voxware rt29hw"); break;
	case 0x77: return("voxware vr12 "); break; case 0x78: return("voxware vr18"); break; case 0x79: return("voxware tq40"); break;
	case 0x80: return("softsound"); break; case 0x81: return("voxware tq60 "); break; case 0x82: return("MS RT24"); break;
	case 0x83: return("G729A"); break; case 0x84: return("MVI_MVI2"); break; case 0x85: return("DF G726"); break;
	case 0x86: return("DF GSM610"); break; case 0x88: return("isaudio"); break; case 0x89: return("onlive"); break;
	case 0x91: return("sbc24"); break; case 0x92: return("dolby ac3 spdif"); break; case 0x97: return("zyxel adpcm"); break;
	case 0x98: return("philips lpcbb"); break; case 0x99: return("packed"); break; case 0x100: return("rhetorex adpcm"); break;
	case 0x101: return("Irat"); break; case 0x102: return("IBM_alaw?"); break; case 0x103: return("IBM_ADPCM?"); break;
	case 0x111: return("vivo G723"); break; case 0x112: return("vivo siren"); break; case 0x123: return("digital g273"); break;
	case 0x200: return("Creative_ADPCM"); break; case 0x202: return("Creative fastspeech 8"); break; 
	case 0x203: return("Creative fastspeech 10"); break;
	case 0x220: return("quarterdeck"); break; case 0x300: return("FM_TOWNS_SND"); break; case 0x400: return("BTV digital"); break;
	case 0x680: return("VME vmpcm"); break; case 0x1000: return("OLIGSM"); break; case 0x1001: return("OLIADPCM"); break;
	case 0x1002: return("OLICELP"); break; case 0x1003: return("OLISBC"); break; case 0x1004: return("OLIOPR"); break;
	case 0x1100: return("LH codec"); break; case 0x1400: return("Norris"); break; case 0x1401: return("isaudio"); break;
	case 0x1500: return("Soundspace musicompression"); break; case 0x2000: return("DVM"); break; 
	}
      break;
    }
  return("unknown"); /* NULL here isn't safe -- Sun segfaults */
}

bool mus_header_no_header(const char *filename)
{
  int chan, bytes;
  bool ok = false;
  chan = mus_file_open_read(filename);
  if (chan == -1) 
    return(mus_error(MUS_CANT_OPEN_FILE, "mus_header: can't open %s: %s", filename, STRERROR(errno)));
  bytes = read(chan, hdrbuf, INITIAL_READ_SIZE);
  CLOSE(chan, filename);
  if (bytes > 4) 
    ok = ((match_four_chars((unsigned char *)hdrbuf, I_DSND)) || 
	  (match_four_chars((unsigned char *)hdrbuf, I_DECN)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_FORM)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_RIFF)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_RIFX)) ||
	  (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_VAX)) || 
	  (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_SUN)) ||
	  (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_MIPS)) || 
	  (equal_big_or_little_endian((unsigned char *)hdrbuf, I_IRCAM_NEXT)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_NIST)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SOUN)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_VOC0)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_AVR_)) ||
	  (mus_char_to_bshort((unsigned char *)hdrbuf) == 1336) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_ALaw)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_GF1P)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_DSIG)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_GOLD)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_Diam)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SRFS)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_CSRE)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_file)) ||
	  ((hdrbuf[0] == 0xf0) && (hdrbuf[1] == 0x7e) && (hdrbuf[3] == 0x01)) ||
	  (equal_big_or_little_endian((unsigned char *)(hdrbuf + 16), 0x00006a1a)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SPIB)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_S___)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_riff)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_PVF1)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_PVF2)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_MThd)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SND_)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_FSMt)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_DDSF)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_LM89)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SY85)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SY80)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_PRAM)) ||
	  (match_four_chars((unsigned char *)(hdrbuf + 35), I_UWFD)) ||
	  (match_four_chars((unsigned char *)(hdrbuf + 76), I_SCRS)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_covox)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I__PAF)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_FAP_)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_TWIN)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_IMPS)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SMP1)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_Maui)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_SDIF)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_ajkg)) ||
	  (match_four_chars((unsigned char *)hdrbuf, I_NVF_)));
  return(!ok);
}

void mus_header_set_aiff_loop_info(int *data)
{
  /* include modes */
  if (data)
    {
      loop_starts[0] = data[0];
      loop_ends[0] = data[1];
      loop_starts[1] = data[2];
      loop_ends[1] = data[3];
      base_note = data[4];
      base_detune = data[5];
      loop_modes[0] = data[6];
      loop_modes[1] = data[7];
    }
  else
    {
      loop_modes[0] = 0;
      loop_modes[1] = 0;
    }
}
