/* IO handlers */
/*
 * --------------------------------
 * int mus_file_read(int fd, int beg, int end, int chans, mus_sample_t **bufs)
 * int mus_file_write(int tfd, int beg, int end, int chans, mus_sample_t **bufs)
 * int mus_file_open_read(const char *arg) 
 * int mus_file_open_write(const char *arg)
 * int mus_file_create(const char *arg)
 * int mus_file_reopen_write(const char *arg)
 * int mus_file_close(int fd)
 * bool mus_file_probe(const char *arg)
 * char *mus_format(const char *format, ...)
 * off_t mus_file_seek_frame(int tfd, off_t frame)
 * --------------------------------
 */

#include <mus-config.h>

#if USE_SND
  #include "snd.h"
#endif

#include <math.h>
#include <stdio.h>
#if HAVE_FCNTL_H
  #include <fcntl.h>
#endif
#if HAVE_LIMITS_H
  #include <limits.h>
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
#include <stdarg.h>

#include "_sndlib.h"

/* data translations for big/little endian machines
 *   the m_* forms are macros where possible for speed (dating back to 1991 -- probably not needed)
 */

void mus_bint_to_char(unsigned char *j, int x)
{
  unsigned char *ox = (unsigned char *)&x;
#if MUS_LITTLE_ENDIAN
  j[0] = ox[3]; j[1] = ox[2]; j[2] = ox[1]; j[3] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 4);
#endif
}

int mus_char_to_bint(const unsigned char *inp)
{
  int o;
  unsigned char *outp = (unsigned char *)&o;
#if MUS_LITTLE_ENDIAN
  outp[0] = inp[3]; outp[1] = inp[2]; outp[2] = inp[1]; outp[3] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 4);
#endif
  return(o);
}

void mus_lint_to_char(unsigned char *j, int x)
{
  unsigned char *ox = (unsigned char *)&x;
#if (!MUS_LITTLE_ENDIAN)
  j[0] = ox[3]; j[1] = ox[2]; j[2] = ox[1]; j[3] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 4);
#endif
}

int mus_char_to_lint(const unsigned char *inp)
{
  int o;
  unsigned char *outp = (unsigned char *)&o;
#if (!MUS_LITTLE_ENDIAN)
  outp[0] = inp[3]; outp[1] = inp[2]; outp[2] = inp[1]; outp[3] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 4);
#endif
  return(o);
}

int mus_char_to_uninterpreted_int(const unsigned char *inp)
{
  int o;
  unsigned char *outp = (unsigned char *)&o;
  memcpy((void *)outp, (void *)inp, 4);
  return(o);
}

unsigned int mus_char_to_ubint(const unsigned char *inp)
{
  unsigned int o;
  unsigned char *outp = (unsigned char *)&o;
#if MUS_LITTLE_ENDIAN
  outp[0] = inp[3]; outp[1] = inp[2]; outp[2] = inp[1]; outp[3] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 4);
#endif
  return(o);
}

unsigned int mus_char_to_ulint(const unsigned char *inp)
{
  unsigned int o;
  unsigned char *outp = (unsigned char *)&o;
#if (!MUS_LITTLE_ENDIAN)
  outp[0] = inp[3]; outp[1] = inp[2]; outp[2] = inp[1]; outp[3] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 4);
#endif
  return(o);
}


void mus_bfloat_to_char(unsigned char *j, float x)
{
  unsigned char *ox = (unsigned char *)&x;
#if MUS_LITTLE_ENDIAN
  j[0] = ox[3]; j[1] = ox[2]; j[2] = ox[1]; j[3] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 4);
#endif
}

float mus_char_to_bfloat(const unsigned char *inp)
{
  float o;
  unsigned char *outp = (unsigned char *)&o;
#if MUS_LITTLE_ENDIAN
  outp[0] = inp[3]; outp[1] = inp[2]; outp[2] = inp[1]; outp[3] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 4);
#endif
  return(o);
}

void mus_lfloat_to_char(unsigned char *j, float x)
{
  unsigned char *ox = (unsigned char *)&x;
#if (!MUS_LITTLE_ENDIAN)
  j[0] = ox[3]; j[1] = ox[2]; j[2] = ox[1]; j[3] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 4);
#endif
}

float mus_char_to_lfloat(const unsigned char *inp)
{
  float o;
  unsigned char *outp = (unsigned char *)&o;
#if (!MUS_LITTLE_ENDIAN)
  outp[0] = inp[3]; outp[1] = inp[2]; outp[2] = inp[1]; outp[3] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 4);
#endif
  return(o);
}

void mus_bshort_to_char(unsigned char *j, short x)
{
  unsigned char *ox = (unsigned char *)&x;
#if MUS_LITTLE_ENDIAN
  j[0] = ox[1]; j[1] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 2); /* I wonder if this is faster */
#endif
}

short mus_char_to_bshort(const unsigned char *inp)
{
  short o;
  unsigned char *outp = (unsigned char *)&o;
#if MUS_LITTLE_ENDIAN
  outp[0] = inp[1]; outp[1] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 2);
#endif
  return(o);
}

void mus_lshort_to_char(unsigned char *j, short x)
{
  unsigned char *ox = (unsigned char *)&x;
#if (!MUS_LITTLE_ENDIAN)
  j[0] = ox[1]; j[1] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 2);
#endif
}

short mus_char_to_lshort(const unsigned char *inp)
{
  short o;
  unsigned char *outp = (unsigned char *)&o;
#if (!MUS_LITTLE_ENDIAN)
  outp[0] = inp[1]; outp[1] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 2);
#endif
  return(o);
}

void mus_ubshort_to_char(unsigned char *j, unsigned short x)
{
  unsigned char *ox = (unsigned char *)&x;
#if MUS_LITTLE_ENDIAN
  j[0] = ox[1]; j[1] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 2);
#endif
}

unsigned short mus_char_to_ubshort(const unsigned char *inp)
{
  unsigned short o;
  unsigned char *outp = (unsigned char *)&o;
#if MUS_LITTLE_ENDIAN
  outp[0] = inp[1]; outp[1] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 2);
#endif
  return(o);
}

void mus_ulshort_to_char(unsigned char *j, unsigned short x)
{
  unsigned char *ox = (unsigned char *)&x;
#if (!MUS_LITTLE_ENDIAN)
  j[0] = ox[1]; j[1] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 2);
#endif
}

unsigned short mus_char_to_ulshort(const unsigned char *inp)
{
  unsigned short o;
  unsigned char *outp = (unsigned char *)&o;
#if (!MUS_LITTLE_ENDIAN)
  outp[0] = inp[1]; outp[1] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 2);
#endif
  return(o);
}

double mus_char_to_ldouble(const unsigned char *inp)
{
  double o;
  unsigned char *outp = (unsigned char *)&o;
#if (MUS_LITTLE_ENDIAN)
  memcpy((void *)outp, (void *)inp, 8);
#else
  outp[0] = inp[7]; outp[1] = inp[6]; outp[2] = inp[5]; outp[3] = inp[4]; outp[4] = inp[3]; outp[5] = inp[2]; outp[6] = inp[1]; outp[7] = inp[0];
#endif
  return(o);
}

double mus_char_to_bdouble(const unsigned char *inp)
{
  double o;
  unsigned char *outp = (unsigned char *)&o;
#if (MUS_LITTLE_ENDIAN)
  outp[0] = inp[7]; outp[1] = inp[6]; outp[2] = inp[5]; outp[3] = inp[4]; outp[4] = inp[3]; outp[5] = inp[2]; outp[6] = inp[1]; outp[7] = inp[0];
#else
  memcpy((void *)outp, (void *)inp, 8);
#endif
  return(o);
}

void mus_bdouble_to_char(unsigned char *j, double x)
{
  unsigned char *ox = (unsigned char *)&x;
#if (MUS_LITTLE_ENDIAN)
  j[0] = ox[7]; j[1] = ox[6]; j[2] = ox[5]; j[3] = ox[4]; j[4] = ox[3]; j[5] = ox[2]; j[6] = ox[1]; j[7] = ox[0];
#else
  memcpy((void *)j, (void *)ox, 8);
#endif
}

void mus_ldouble_to_char(unsigned char *j, double x)
{
  unsigned char *ox = (unsigned char *)&x;
#if (MUS_LITTLE_ENDIAN)
  memcpy((void *)j, (void *)ox, 8);
#else
  j[0] = ox[7]; j[1] = ox[6]; j[2] = ox[5]; j[3] = ox[4]; j[4] = ox[3]; j[5] = ox[2]; j[6] = ox[1]; j[7] = ox[0];
#endif
}

#if HAVE_BYTESWAP_H
  #include <byteswap.h>
#endif

#if MUS_LITTLE_ENDIAN

  #if HAVE_BYTESWAP_H
    #define m_big_endian_short(n)                  ((short)(bswap_16((*((unsigned short *)n)))))
    #define m_big_endian_int(n)                    ((int)(bswap_32((*((unsigned int *)n)))))
    #define m_big_endian_unsigned_short(n)         ((unsigned short)(bswap_16((*((unsigned short *)n)))))
  #else
    #define m_big_endian_short(n)                  (mus_char_to_bshort(n))
    #define m_big_endian_int(n)                    (mus_char_to_bint(n))
    #define m_big_endian_unsigned_short(n)         (mus_char_to_ubshort(n))
  #endif
  #define m_big_endian_float(n)                    (mus_char_to_bfloat(n))
  #define m_big_endian_double(n)                   (mus_char_to_bdouble(n))

  #define m_little_endian_short(n)                 (*((short *)n))
  #define m_little_endian_int(n)                   (*((int *)n))
  #define m_little_endian_float(n)                 (*((float *)n))
  #define m_little_endian_double(n)                (*((double *)n))
  #define m_little_endian_unsigned_short(n)        (*((unsigned short *)n))

  #if HAVE_BYTESWAP_H
    #define m_set_big_endian_short(n, x)           (*((short *)n)) = ((short)(bswap_16(x)))
    #define m_set_big_endian_int(n, x)             (*((int *)n)) = ((int)(bswap_32(x)))
    #define m_set_big_endian_unsigned_short(n, x)  (*((unsigned short *)n)) = ((unsigned short)(bswap_16(x)))
  #else
    #define m_set_big_endian_short(n, x)           mus_bshort_to_char(n, x)
    #define m_set_big_endian_int(n, x)             mus_bint_to_char(n, x)
    #define m_set_big_endian_unsigned_short(n, x)  mus_ubshort_to_char(n, x)
  #endif
  #define m_set_big_endian_float(n, x)             mus_bfloat_to_char(n, x)
  #define m_set_big_endian_double(n, x)            mus_bdouble_to_char(n, x)

  #define m_set_little_endian_short(n, x)          (*((short *)n)) = x
  #define m_set_little_endian_int(n, x)            (*((int *)n)) = x
  #define m_set_little_endian_float(n, x)          (*((float *)n)) = x
  #define m_set_little_endian_double(n, x)         (*((double *)n)) = x
  #define m_set_little_endian_unsigned_short(n, x) (*((unsigned short *)n)) = x

#else

  #ifndef MUS_SUN
    #define m_big_endian_short(n)                  (*((short *)n))
    #define m_big_endian_int(n)                    (*((int *)n))
    #define m_big_endian_float(n)                  (*((float *)n))
    #define m_big_endian_double(n)                 (*((double *)n))
    #define m_big_endian_unsigned_short(n)         (*((unsigned short *)n))

    #define m_set_big_endian_short(n, x)           (*((short *)n)) = x
    #define m_set_big_endian_int(n, x)             (*((int *)n)) = x
    #define m_set_big_endian_float(n, x)           (*((float *)n)) = x
    #define m_set_big_endian_double(n, x)          (*((double *)n)) = x
    #define m_set_big_endian_unsigned_short(n, x)  (*((unsigned short *)n)) = x
  #else
    #define m_big_endian_short(n)                  (mus_char_to_bshort(n))
    #define m_big_endian_int(n)                    (mus_char_to_bint(n))
    #define m_big_endian_float(n)                  (mus_char_to_bfloat(n))
    #define m_big_endian_double(n)                 (mus_char_to_bdouble(n))
    #define m_big_endian_unsigned_short(n)         (mus_char_to_ubshort(n))

    #define m_set_big_endian_short(n, x)           mus_bshort_to_char(n, x)
    #define m_set_big_endian_int(n, x)             mus_bint_to_char(n, x)
    #define m_set_big_endian_float(n, x)           mus_bfloat_to_char(n, x)
    #define m_set_big_endian_double(n, x)          mus_bdouble_to_char(n, x)
    #define m_set_big_endian_unsigned_short(n, x)  mus_ubshort_to_char(n, x)
  #endif

  #if HAVE_BYTESWAP_H
    #define m_little_endian_short(n)               ((short)(bswap_16((*((unsigned short *)n)))))
    #define m_little_endian_int(n)                 ((int)(bswap_32((*((unsigned int *)n)))))
    #define m_little_endian_unsigned_short(n)      ((unsigned short)(bswap_16((*((unsigned short *)n)))))
  #else
    #define m_little_endian_short(n)               (mus_char_to_lshort(n))
    #define m_little_endian_int(n)                 (mus_char_to_lint(n))
    #define m_little_endian_unsigned_short(n)      (mus_char_to_ulshort(n))
  #endif
  #define m_little_endian_float(n)                 (mus_char_to_lfloat(n))
  #define m_little_endian_double(n)                (mus_char_to_ldouble(n))

  #if HAVE_BYTESWAP_H
    #define m_set_little_endian_short(n, x)        (*((short *)n)) = ((short)(bswap_16(x)))
    #define m_set_little_endian_int(n, x)          (*((int *)n)) = ((int)(bswap_32(x)))
    #define m_set_little_endian_unsigned_short(n, x) (*((unsigned short *)n)) = ((unsigned short)(bswap_16(x)))
  #else
    #define m_set_little_endian_short(n, x)        mus_lshort_to_char(n, x)
    #define m_set_little_endian_int(n, x)          mus_lint_to_char(n, x)
    #define m_set_little_endian_unsigned_short(n, x) mus_ulshort_to_char(n, x)
  #endif
  #define m_set_little_endian_float(n, x)          mus_lfloat_to_char(n, x)
  #define m_set_little_endian_double(n, x)         mus_ldouble_to_char(n, x)

#endif


/* ---------------- file descriptors ---------------- */

typedef struct {
  char *name;
  int data_format, bytes_per_sample, chans, header_type;
  bool clipping;
  off_t data_location;
  Float prescaler;
} io_fd;

static int io_fd_size = 0;
static io_fd **io_fds = NULL;
#define IO_FD_ALLOC_SIZE 8
static bool clipping_default = false;
static Float prescaler_default = 1.0;

bool mus_clipping(void) {return(clipping_default);}
bool mus_set_clipping(bool new_value) {clipping_default = new_value; return(new_value);}
Float mus_prescaler(void) {return(prescaler_default);}
Float mus_set_prescaler(Float new_value) {prescaler_default = new_value; return(new_value);}

int mus_file_open_descriptors(int tfd, const char *name, int format, int size /* datum size */, off_t location, int chans, int type)
{
  io_fd *fd;
  int i, lim = -1;
  if (io_fd_size == 0)
    {
      io_fd_size = tfd + IO_FD_ALLOC_SIZE;
      io_fds = (io_fd **)CALLOC(io_fd_size, sizeof(io_fd *));
      if (!io_fds) return(MUS_MEMORY_ALLOCATION_FAILED);
    }
  if (io_fd_size <= tfd)
    {
      lim = io_fd_size;
      io_fd_size = tfd + IO_FD_ALLOC_SIZE;
      io_fds = (io_fd **)REALLOC(io_fds, io_fd_size * sizeof(io_fd *));
      for (i = lim; i < io_fd_size; i++) io_fds[i] = NULL;
    }
  if (io_fds[tfd] == NULL)
    {
      io_fds[tfd] = (io_fd *)CALLOC(1, sizeof(io_fd));
      if (!(io_fds[tfd])) return(MUS_MEMORY_ALLOCATION_FAILED);
    }

  fd = io_fds[tfd];
  fd->data_format = format;
  fd->bytes_per_sample = size;
#if MUS_DEBUGGING
  if (size != mus_bytes_per_sample(format))
    fprintf(stderr, "format trouble in mus_file_open_descriptors: %d != %d\n", size, mus_bytes_per_sample(format));
#endif
  fd->data_location = location;
  fd->clipping = clipping_default;
  fd->prescaler = prescaler_default;
  fd->header_type = type;
  fd->chans = chans;
  if (name)
    {
      fd->name = (char *)CALLOC(strlen(name) + 1, sizeof(char));
      strcpy(fd->name, name);
    }
  return(MUS_NO_ERROR);
}

bool mus_file_clipping(int tfd)
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(false);
  fd = io_fds[tfd];
  return(fd->clipping);
}

int mus_file_set_clipping(int tfd, bool clipped)
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED);
  fd = io_fds[tfd];
  fd->clipping = clipped;
  return(MUS_NO_ERROR);
}

int mus_file_set_header_type(int tfd, int type)
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED);
  fd = io_fds[tfd];
  fd->header_type = type;
  return(MUS_NO_ERROR);
}

int mus_file_header_type(int tfd)
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED);
  fd = io_fds[tfd];
  return(fd->header_type);
}

Float mus_file_prescaler(int tfd) 
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(0.0);
  fd = io_fds[tfd];
  return(fd->prescaler);
}

Float mus_file_set_prescaler(int tfd, Float val) 
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(0.0);
  fd = io_fds[tfd];
  fd->prescaler = val; 
  return(val);
}

char *mus_file_fd_name(int tfd)
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(NULL);
  fd = io_fds[tfd];
  return(fd->name);
}

int mus_file_set_chans(int tfd, int chans)
{
  io_fd *fd;
  if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL)) return(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED);
  fd = io_fds[tfd];
  fd->chans = chans;
  return(MUS_NO_ERROR);
}


/* ---------------- open, creat, close ---------------- */

int mus_file_open_read(const char *arg) 
{
  int fd;
#ifdef MUS_WINDOZE
  fd = OPEN(arg, O_RDONLY | O_BINARY, 0);
#else
  fd = OPEN(arg, O_RDONLY, 0);
#endif
  return(fd);
}

bool mus_file_probe(const char *arg) 
{
#if HAVE_ACCESS
  return(access(arg, F_OK) == 0);
#else
  int fd;
#ifdef O_NONBLOCK
  fd = OPEN(arg, O_RDONLY, O_NONBLOCK);
#else
  fd = OPEN(arg, O_RDONLY, 0);
#endif
  if (fd == -1) return(false);
  CLOSE(fd, arg);
  return(true);
#endif
}

int mus_file_open_write(const char *arg)
{
  int fd;
#ifdef MUS_WINDOZE
  if ((fd = OPEN(arg, O_RDWR | O_BINARY, 0)) == -1)
#else
  if ((fd = OPEN(arg, O_RDWR, 0)) == -1)
#endif
    fd = CREAT(arg, 0666);  /* equivalent to the new open(arg, O_RDWR | O_CREAT | O_TRUNC, 0666) */
  else lseek(fd, 0L, SEEK_END);
  return(fd);
}

int mus_file_create(const char *arg)
{
  return(CREAT(arg, 0666));
}

int mus_file_reopen_write(const char *arg)
{
  int fd;
#ifdef MUS_WINDOZE
  fd = OPEN(arg, O_RDWR | O_BINARY, 0);
#else
  fd = OPEN(arg, O_RDWR, 0);
#endif
  return(fd);
}

int mus_file_close(int fd)
{
  io_fd *fdp;
  int close_result = 0;
  if ((io_fds == NULL) || (fd >= io_fd_size) || (fd < 0) || (io_fds[fd] == NULL)) return(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED);
  fdp = io_fds[fd];
#if USE_SND
  CLOSE(fd, fdp->name);
#else
  close_result = close(fd);
#endif
  if (fdp->name) {FREE(fdp->name); fdp->name = NULL;}
  FREE(fdp);
  io_fds[fd] = NULL;
  if (close_result < 0)
    return(MUS_CANT_CLOSE_FILE);
  return(MUS_NO_ERROR);
}



/* ---------------- seek ---------------- */

off_t mus_file_seek_frame(int tfd, off_t frame)
{
  io_fd *fd;
  if (io_fds == NULL) 
    return(mus_error(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED, "mus_file_seek_frame: no file descriptors!"));
  if (tfd < 0)
    return(mus_error(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED, "mus_file_seek_frame: file descriptor = %d?", tfd));
  if ((tfd >= io_fd_size) || 
      (io_fds[tfd] == NULL))
    return(mus_error(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED,
		     "mus_file_seek_frame: file descriptors not realloc'd? (tfd: %d, io_fd_size: %d)", tfd, io_fd_size));
  fd = io_fds[tfd];
  if (fd->data_format == MUS_UNKNOWN) 
    return(mus_error(MUS_NOT_A_SOUND_FILE, "mus_file_seek_frame: invalid data format for %s", fd->name));
  return(lseek(tfd, fd->data_location + (fd->chans * frame * fd->bytes_per_sample), SEEK_SET));
}



/* ---------------- mulaw/alaw conversions ----------------
 *
 *      x : input signal with max value 32767
 *     mu : compression parameter (mu = 255 used for telephony)
 *     y = (32767/log(1+mu))*log(1+mu*abs(x)/32767)*sign(x); -- this isn't right -- typo?
 */

/* from sox g711.c */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */

static short seg_end[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF,  0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};

static int search(int val, short *table, int size)
{
  int i;
  for (i = 0; i < size; i++) {if (val <= *table++) return (i);}
  return (size);
}

static unsigned char to_alaw(int pcm_val)
{
  int mask, seg;
  if (pcm_val >= 0) mask = 0xD5; else {mask = 0x55; pcm_val = -pcm_val - 8;}
  seg = search(pcm_val, seg_end, 8);
  if (seg >= 8)	return (0x7F ^ mask);
  else 
    {
      unsigned char	aval;
      aval = seg << SEG_SHIFT;
      if (seg < 2) aval |= (pcm_val >> 4) & QUANT_MASK; else aval |= (pcm_val >> (seg + 3)) & QUANT_MASK;
      return (aval ^ mask);
    }
}

static const int alaw[256] = {
 -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736, -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784, 
 -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368, -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392, 
 -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944, -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136, 
 -11008, -10496, -12032, -11520, -8960, -8448, -9984, -9472, -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568, 
 -344, -328, -376, -360, -280, -264, -312, -296, -472, -456, -504, -488, -408, -392, -440, -424, 
 -88, -72, -120, -104, -24, -8, -56, -40, -216, -200, -248, -232, -152, -136, -184, -168, 
 -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184, -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696, 
 -688, -656, -752, -720, -560, -528, -624, -592, -944, -912, -1008, -976, -816, -784, -880, -848, 
 5504, 5248, 6016, 5760, 4480, 4224, 4992, 4736, 7552, 7296, 8064, 7808, 6528, 6272, 7040, 6784, 
 2752, 2624, 3008, 2880, 2240, 2112, 2496, 2368, 3776, 3648, 4032, 3904, 3264, 3136, 3520, 3392, 
 22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944, 30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136, 
 11008, 10496, 12032, 11520, 8960, 8448, 9984, 9472, 15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568, 
 344, 328, 376, 360, 280, 264, 312, 296, 472, 456, 504, 488, 408, 392, 440, 424, 
 88, 72, 120, 104, 24, 8, 56, 40, 216, 200, 248, 232, 152, 136, 184, 168, 
 1376, 1312, 1504, 1440, 1120, 1056, 1248, 1184, 1888, 1824, 2016, 1952, 1632, 1568, 1760, 1696, 
 688, 656, 752, 720, 560, 528, 624, 592, 944, 912, 1008, 976, 816, 784, 880, 848
};

#define	BIAS		(0x84)		/* Bias for linear code. */

static unsigned char to_mulaw(int pcm_val)
{
  int mask;
  int seg;
  if (pcm_val < 0) {pcm_val = BIAS - pcm_val; mask = 0x7F;} else {pcm_val += BIAS; mask = 0xFF;}
  seg = search(pcm_val, seg_end, 8);
  if (seg >= 8) return (0x7F ^ mask);
  else 
    {
      unsigned char	uval;
      uval = (seg << 4) | ((pcm_val >> (seg + 3)) & 0xF);
      return (uval ^ mask);
    }
}

/* generated by SNDiMulaw on a NeXT */
static const int mulaw[256] = {
  -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956, -23932, -22908, -21884, -20860, 
  -19836, -18812, -17788, -16764, -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412, 
  -11900, -11388, -10876, -10364, -9852, -9340, -8828, -8316, -7932, -7676, -7420, -7164, -6908, 
  -6652, -6396, -6140, -5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092, -3900, -3772, -3644, 
  -3516, -3388, -3260, -3132, -3004, -2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980, -1884, 
  -1820, -1756, -1692, -1628, -1564, -1500, -1436, -1372, -1308, -1244, -1180, -1116, -1052, -988, 
  -924, -876, -844, -812, -780, -748, -716, -684, -652, -620, -588, -556, -524, -492, -460, -428, 
  -396, -372, -356, -340, -324, -308, -292, -276, -260, -244, -228, -212, -196, -180, -164, -148, 
  -132, -120, -112, -104, -96, -88, -80, -72, -64, -56, -48, -40, -32, -24, -16, -8, 0, 32124, 31100, 
  30076, 29052, 28028, 27004, 25980, 24956, 23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764, 
  15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412, 11900, 11388, 10876, 10364, 9852, 9340, 
  8828, 8316, 7932, 7676, 7420, 7164, 6908, 6652, 6396, 6140, 5884, 5628, 5372, 5116, 4860, 4604, 
  4348, 4092, 3900, 3772, 3644, 3516, 3388, 3260, 3132, 3004, 2876, 2748, 2620, 2492, 2364, 2236, 
  2108, 1980, 1884, 1820, 1756, 1692, 1628, 1564, 1500, 1436, 1372, 1308, 1244, 1180, 1116, 1052, 
  988, 924, 876, 844, 812, 780, 748, 716, 684, 652, 620, 588, 556, 524, 492, 460, 428, 396, 372, 
  356, 340, 324, 308, 292, 276, 260, 244, 228, 212, 196, 180, 164, 148, 132, 120, 112, 104, 96, 
  88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0};



/* ---------------- read ---------------- */

#define BUFLIM (64 * 1024)

#if SNDLIB_USE_FLOATS
  #define MUS_SAMPLE_UNSCALED(n) ((n) / 32768.0)
#else
  #define MUS_SAMPLE_UNSCALED(n) ((n) * (1 << (MUS_SAMPLE_BITS - 16)))
#endif

static int mus_read_any_1(int tfd, int beg, int chans, int nints, mus_sample_t **bufs, mus_sample_t **cm, char *inbuf)
{
  int loclim;
  io_fd *fd;
  int bytes, j, lim, siz, total, leftover, total_read, k, loc, oldloc, siz_chans, buflim, format;
  unsigned char *jchar;
  char *charbuf = NULL;
  mus_sample_t *buffer;
  float prescaling;
  bool from_buffer = false;
  if (nints <= 0) return(0);
  if (inbuf) from_buffer = true;
  if (!from_buffer)
    {
      if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL))
	return(mus_error(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED, "mus_read: no file descriptors!"));
      fd = io_fds[tfd];
      if (fd->data_format == MUS_UNKNOWN) 
	return(mus_error(MUS_FILE_CLOSED, "mus_read: invalid data format for %s", fd->name));
      format = fd->data_format;
      siz = fd->bytes_per_sample;
      if ((format == MUS_OUT_FORMAT) && 
	  (chans == 1) && 
	  (beg == 0)
#if SNDLIB_USE_FLOATS 
	  && (fd->prescaler == 1.0)
#endif
	  )
	{
	  bytes = nints * siz;
	  total = read(tfd, (char *)(bufs[0]), bytes);
	  if (total != bytes)
	    {
	      if (total <= 0)
		memset((void *)(bufs[0]), 0, bytes);
	      else
		{
		  int i, last;
		  last = beg + nints;
		  for (i = total / siz; i < last; i++)
		    bufs[0][i] = MUS_SAMPLE_0;
		}
	    }
	  return(total / siz);
	}

      prescaling = (float)(fd->prescaler * MUS_FLOAT_TO_SAMPLE(1.0));
      /* not MUS_FLOAT_TO_SAMPLE(fd->prescaler) here because there's a possible cast to int which can overflow */

      charbuf = (char *)CALLOC(BUFLIM, sizeof(char)); 
      if (charbuf == NULL) 
	return(mus_error(MUS_MEMORY_ALLOCATION_FAILED, "mus_read: IO buffer allocation failed"));
    }
  else
    {
      charbuf = inbuf;
      siz = mus_bytes_per_sample(tfd);
      prescaling = (float)(MUS_FLOAT_TO_SAMPLE(1.0));
      format = tfd;
    }
  siz_chans = siz * chans;
  leftover = (nints * siz_chans);
  k = (BUFLIM) % siz_chans;
  if (k != 0) /* for example, 3 channel output of 1-byte (mulaw) samples will need a mod 3 buffer */
    buflim = (BUFLIM) - k;
  else buflim = BUFLIM;
  total_read = 0;
  loc = beg;
  while (leftover > 0)
    {
      bytes = leftover;
      if (bytes > buflim) 
	{
	  leftover = (bytes - buflim); 
	  bytes = buflim;
	} 
      else leftover = 0;
      if (!from_buffer)
	{
	  total = read(tfd, charbuf, bytes); 
	  if (total <= 0) 
	    {
	      /* zero out trailing section (some callers don't check the returned value) -- this added 9-May-99 */

	      lim = beg + nints;
	      if (loc < lim)
		for (k = 0; k < chans; k++)
		  if ((cm == NULL) || (cm[k]))
		    {
		      if (loc == 0)
			memset((void *)(bufs[k]), 0, lim * sizeof(mus_sample_t));
		      else
			for (j = loc; j < lim; j++) 
			  bufs[k][j] = MUS_SAMPLE_0;
		    }
	      FREE(charbuf);
	      return(total_read);
	    }
	  lim = (int) (total / siz_chans);  /* this divide must be exact (hence the buflim calc above) */
	}
      else
	{
	  lim = nints; /* frames in this case */
	  leftover = 0;
	}
      total_read += lim;
      oldloc = loc;

      for (k = 0; k < chans; k++)
	{
	  if ((cm == NULL) || (cm[k]))
	    {
	      buffer = (mus_sample_t *)(bufs[k]);
	      if (buffer)
		{
		  loc = oldloc;
		  loclim = loc + lim;
		  jchar = (unsigned char *)charbuf;
		  jchar += (k * siz);
		  switch (format)
		    {
		    case MUS_BSHORT:               
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_SHORT_TO_SAMPLE(m_big_endian_short(jchar)); 
		      break;
		    case MUS_LSHORT: 
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_SHORT_TO_SAMPLE(m_little_endian_short(jchar)); 
		      break;
		    case MUS_BINT:              
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_INT_TO_SAMPLE(m_big_endian_int(jchar)); 
		      break;
		    case MUS_LINT: 
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_INT_TO_SAMPLE(m_little_endian_int(jchar)); 
		      break;
		    case MUS_BINTN:              
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_INT_TO_SAMPLE((m_big_endian_int(jchar) >> (32 - MUS_SAMPLE_BITS)));
		      break;
		    case MUS_LINTN: 
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_INT_TO_SAMPLE((m_little_endian_int(jchar) >> (32 - MUS_SAMPLE_BITS)));
		      break;
		    case MUS_MULAW:  	              
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_SHORT_TO_SAMPLE(mulaw[*jchar]); 
		      break;
		    case MUS_ALAW:                  
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_SHORT_TO_SAMPLE(alaw[*jchar]); 
		      break;
		    case MUS_BYTE:                
		      for (; loc < loclim; loc++, jchar += siz_chans)
			buffer[loc] = MUS_BYTE_TO_SAMPLE((signed char)(*jchar));
		      break;
		    case MUS_UBYTE:     	      
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_BYTE_TO_SAMPLE((int)(*jchar) - 128);
		      break;
		    case MUS_BFLOAT:
		      if (prescaling == 1.0)
			{
			  for (; loc < loclim; loc++, jchar += siz_chans) 
			    buffer[loc] = (mus_sample_t) (m_big_endian_float(jchar));
			}
		      else
			{
			  for (; loc < loclim; loc++, jchar += siz_chans) 
			    buffer[loc] = (mus_sample_t) (prescaling * (m_big_endian_float(jchar)));
			}
		      break;
		    case MUS_BFLOAT_UNSCALED:
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = (mus_sample_t) (MUS_SAMPLE_UNSCALED(m_big_endian_float(jchar)));
		      break;
		    case MUS_BDOUBLE:   
		      for (; loc < loclim; loc++, jchar += siz_chans)
			buffer[loc] = (mus_sample_t) (prescaling * (m_big_endian_double(jchar)));
		      break;
		    case MUS_BDOUBLE_UNSCALED:   
		      for (; loc < loclim; loc++, jchar += siz_chans)
			buffer[loc] = (mus_sample_t) (MUS_SAMPLE_UNSCALED(m_big_endian_double(jchar)));
		      break;
		    case MUS_LFLOAT:
		      if (prescaling == 1.0)
			{
			  for (; loc < loclim; loc++, jchar += siz_chans) 
			    buffer[loc] = (mus_sample_t) (m_little_endian_float(jchar));
			}
		      else
			{
			  for (; loc < loclim; loc++, jchar += siz_chans) 
			    buffer[loc] = (mus_sample_t) (prescaling * (m_little_endian_float(jchar)));
			}
		      break;
		    case MUS_LFLOAT_UNSCALED:    
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = (mus_sample_t) (MUS_SAMPLE_UNSCALED(m_little_endian_float(jchar)));
		      break;
		    case MUS_LDOUBLE:   
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = (mus_sample_t) (prescaling * (m_little_endian_double(jchar)));
		      break;
		    case MUS_LDOUBLE_UNSCALED:   
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = (mus_sample_t) (MUS_SAMPLE_UNSCALED(m_little_endian_double(jchar)));
		      break;
		    case MUS_UBSHORT:   
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_SHORT_TO_SAMPLE((int)(m_big_endian_unsigned_short(jchar)) - 32768);
		      break;
		    case MUS_ULSHORT:   
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_SHORT_TO_SAMPLE((int)(m_little_endian_unsigned_short(jchar)) - 32768);
		      break;
		    case MUS_B24INT:
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_INT24_TO_SAMPLE((int)(((jchar[0] << 24) + 
								 (jchar[1] << 16) + 
								 (jchar[2] << 8)) >> 8));
		      break;
		    case MUS_L24INT:   
		      for (; loc < loclim; loc++, jchar += siz_chans) 
			buffer[loc] = MUS_INT24_TO_SAMPLE((int)(((jchar[2] << 24) + 
								 (jchar[1] << 16) + 
								 (jchar[0] << 8)) >> 8));
		      break;
		    }
		}
	    }
	}
    }
  if (!from_buffer) FREE(charbuf);
  return(total_read);
}

int mus_file_read_any(int tfd, int beg, int chans, int nints, mus_sample_t **bufs, mus_sample_t **cm)
{
  return(mus_read_any_1(tfd, beg, chans, nints, bufs, cm, NULL));
}

int mus_file_read_file(int tfd, int beg, int chans, int nints, mus_sample_t **bufs)
{
  return(mus_read_any_1(tfd, beg, chans, nints, bufs, NULL, NULL));
}

int mus_file_read_buffer(int charbuf_data_format, int beg, int chans, int nints, mus_sample_t **bufs, char *charbuf)
{
  return(mus_read_any_1(charbuf_data_format, beg, chans, nints, bufs, NULL, charbuf)); 
}

int mus_file_read(int tfd, int beg, int end, int chans, mus_sample_t **bufs)
{
  int num, rtn, i, k;
  num = (end - beg + 1);
  rtn = mus_read_any_1(tfd, beg, chans, num, bufs, NULL, NULL);
  if (rtn == MUS_ERROR) return(MUS_ERROR);
  if (rtn < num) 
    /* this zeroing can be fooled if the file is chunked and has trailing, non-data chunks */
    for (k = 0; k < chans; k++)
      {
	mus_sample_t *buffer;
	buffer = bufs[k];
	i = rtn + beg;
	/* this happens routinely in mus_outa + initial write (reads ahead in effect) */
	memset((void *)(buffer + i), 0, (end - i + 1) * sizeof(mus_sample_t));
      }
  return(num);
}

int mus_file_read_chans(int tfd, int beg, int end, int chans, mus_sample_t **bufs, mus_sample_t **cm)
{
  /* an optimization of mus_file_read -- just reads the desired channels */
  int num, rtn, i, k;
  num = (end - beg + 1);
  rtn = mus_read_any_1(tfd, beg, chans, num, bufs, cm, NULL);
  if (rtn == MUS_ERROR) return(MUS_ERROR);
  if (rtn < num) 
    for (k = 0; k < chans; k++)
      if ((cm == NULL) || (cm[k]))
	{
	  mus_sample_t *buffer;
	  buffer = bufs[k];
	  i = rtn + beg;
	  memset((void *)(buffer + i), 0, (end - i + 1) * sizeof(mus_sample_t));
	}
  return(num);
}


/* ---------------- write ---------------- */

static int checked_write(int tfd, char *buf, int chars)
{
  int bytes;
  bytes = write(tfd, buf, chars);
  if (bytes != chars) 
    {
      io_fd *fd;
      if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL))
	return(mus_error(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED, "mus_write: no file descriptors!"));
      fd = io_fds[tfd];
      if (fd->data_format == MUS_UNKNOWN) 
	return(mus_error(MUS_FILE_CLOSED,
			 "attempt to write closed file %s",
			 fd->name));
      else
	return(mus_error(MUS_WRITE_ERROR,
			 "mus_write: write error for %s%s%s: only %d of %d bytes written",
			 fd->name, (errno) ? ": " : "", (errno) ? STRERROR(errno) : "",
			 bytes, chars));
    }
  return(MUS_NO_ERROR);
}

static int mus_write_1(int tfd, int beg, int end, int chans, mus_sample_t **bufs, char *inbuf, bool clipped)
{
  int loclim, c3;
  int err;
  io_fd *fd;
  int bytes, j, k, lim, siz, leftover, loc, bk, val, oldloc, buflim, siz_chans, cliploc, data_format;
  bool clipping = false;
  unsigned char *jchar;
  char *charbuf = NULL;
  bool to_buffer = false;
  mus_sample_t *buffer;
  if (chans <= 0) return(0);
  if (inbuf) to_buffer = true;
  if (!to_buffer)
    {
      if ((io_fds == NULL) || (tfd >= io_fd_size) || (tfd < 0) || (io_fds[tfd] == NULL))
	return(mus_error(MUS_FILE_DESCRIPTORS_NOT_INITIALIZED, "mus_write: no file descriptors!"));
      fd = io_fds[tfd];
      if (fd->data_format == MUS_UNKNOWN) 
	return(mus_error(MUS_FILE_CLOSED, "mus_write: invalid data format for %s", fd->name));

      siz = fd->bytes_per_sample;
      data_format = fd->data_format;
      clipping = fd->clipping;

      if ((data_format == MUS_OUT_FORMAT) && (chans == 1) && (!clipping) && (beg == 0))
	{
	  bytes = (end + 1) * siz;
	  return(checked_write(tfd, (char *)(bufs[0]), bytes));
	}
      charbuf = (char *)CALLOC(BUFLIM, sizeof(char)); 
      if (charbuf == NULL) 
	return(mus_error(MUS_MEMORY_ALLOCATION_FAILED, "mus_write: IO buffer allocation failed"));
    }
  else
    {
      charbuf = inbuf;
      siz = mus_bytes_per_sample(tfd);
      data_format = tfd; /* in this case, tfd is the data format (see mus_file_write_buffer below) -- this should be changed! */
      clipping = clipped;
    }
  lim = (end - beg + 1);
  siz_chans = siz * chans;
  leftover = lim * siz_chans;
  k = (BUFLIM) % siz_chans;
  if (k != 0) 
    buflim = (BUFLIM) - k;
  else buflim = BUFLIM;
  loc = beg;
  while (leftover > 0)
    {
      bytes = leftover;
      if (bytes > buflim) 
	{
	  leftover = (bytes - buflim); 
	  bytes = buflim;
	} 
      else leftover = 0;
      lim = (int)(bytes / siz_chans); /* see note above */
      oldloc = loc;

      for (k = 0; k < chans; k++)
	{
	  if (bufs[k] == NULL) continue;
	  loc = oldloc;
	  buffer = (mus_sample_t *)(bufs[k]);
	  if (clipping)
	    {
	      cliploc = oldloc;
	      for (j = 0; j < lim; j++, cliploc++)
		if (buffer[cliploc] > MUS_SAMPLE_MAX)
		  buffer[cliploc] = MUS_SAMPLE_MAX;
		else
		  if (buffer[cliploc] < MUS_SAMPLE_MIN)
		    buffer[cliploc] = MUS_SAMPLE_MIN;
	    }
	  loclim = loc + lim;
	  jchar = (unsigned char *)charbuf; /* if to_buffer we should add the loop offset here, or never loop */
	  jchar += (k * siz); 
	  switch (data_format)
	    {
	    case MUS_BSHORT: 
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_short(jchar, MUS_SAMPLE_TO_SHORT(buffer[loc]));
	      break;
	    case MUS_LSHORT:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_short(jchar, MUS_SAMPLE_TO_SHORT(buffer[loc]));
	      break;
	    case MUS_BINT:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_int(jchar, MUS_SAMPLE_TO_INT(buffer[loc]));
	      break;
	    case MUS_LINT:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_int(jchar, MUS_SAMPLE_TO_INT(buffer[loc]));
	      break;
	    case MUS_BINTN:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_int(jchar, MUS_SAMPLE_TO_INT(buffer[loc]) << (32 - MUS_SAMPLE_BITS));
	      break;
	    case MUS_LINTN:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_int(jchar, MUS_SAMPLE_TO_INT(buffer[loc]) << (32 - MUS_SAMPLE_BITS));
	      break;
	    case MUS_MULAW:     
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		(*jchar) = to_mulaw(MUS_SAMPLE_TO_SHORT(buffer[loc]));
	      break;
	    case MUS_ALAW:      
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		(*jchar) = to_alaw(MUS_SAMPLE_TO_SHORT(buffer[loc]));
	      break;
	    case MUS_BYTE:    
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		(*((signed char *)jchar)) = MUS_SAMPLE_TO_BYTE(buffer[loc]);
	      break;
	    case MUS_UBYTE:  
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		(*jchar) = MUS_SAMPLE_TO_BYTE(buffer[loc]) + 128;
	      break;
	    case MUS_BFLOAT:    
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_float(jchar, MUS_SAMPLE_TO_FLOAT(buffer[loc]));
	      break;
	    case MUS_LFLOAT:    
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_float(jchar, MUS_SAMPLE_TO_FLOAT(buffer[loc]));
	      break;
	    case MUS_BDOUBLE:
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_double(jchar, MUS_SAMPLE_TO_DOUBLE(buffer[loc]));
	      break;
	    case MUS_LDOUBLE:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_double(jchar, MUS_SAMPLE_TO_DOUBLE(buffer[loc]));
	      break;
	    case MUS_BFLOAT_UNSCALED:    
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_float(jchar, 32768.0 * MUS_SAMPLE_TO_FLOAT(buffer[loc]));
	      break;
	    case MUS_LFLOAT_UNSCALED:    
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_float(jchar, 32768.0 * MUS_SAMPLE_TO_FLOAT(buffer[loc]));
	      break;
	    case MUS_BDOUBLE_UNSCALED:
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_double(jchar, 32768.0 * MUS_SAMPLE_TO_DOUBLE(buffer[loc]));
	      break;
	    case MUS_LDOUBLE_UNSCALED:   
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_double(jchar, 32768.0 * MUS_SAMPLE_TO_DOUBLE(buffer[loc]));
	      break;
	    case MUS_UBSHORT: 
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_big_endian_unsigned_short(jchar, (unsigned short)(MUS_SAMPLE_TO_SHORT(buffer[loc]) + 32768));
	      break;
	    case MUS_ULSHORT: 
	      for (; loc < loclim; loc++, jchar += siz_chans) 
		m_set_little_endian_unsigned_short(jchar, (unsigned short)(MUS_SAMPLE_TO_SHORT(buffer[loc]) + 32768));
	      break;
	    case MUS_B24INT:   
	      bk = (k * 3);
	      c3 = chans * 3;
	      for (; loc < loclim; loc++, bk += c3) 
		{
		  val = MUS_SAMPLE_TO_INT24(buffer[loc]);
		  charbuf[bk] = (val >> 16); 
		  charbuf[bk + 1] = (val >> 8); 
		  charbuf[bk + 2] = (val & 0xFF); 
		}
	      break;
	    case MUS_L24INT:   
	      bk = (k * 3);
	      c3 = chans * 3;
	      for (; loc < loclim; loc++, bk += c3)
		{
		  val = MUS_SAMPLE_TO_INT24(buffer[loc]);
		  charbuf[bk + 2] = (val >> 16); 
		  charbuf[bk + 1] = (val >> 8); 
		  charbuf[bk] = (val & 0xFF); 
		}
	      break;
	    }
	}
      if (!to_buffer)
	{
	  err = checked_write(tfd, charbuf, bytes);
	  if (err == MUS_ERROR) 
	    {
	      FREE(charbuf); 
	      return(MUS_ERROR);
	    }
	}
    }
  if (!to_buffer) FREE(charbuf);
  return(MUS_NO_ERROR);
}

int mus_file_write(int tfd, int beg, int end, int chans, mus_sample_t **bufs)
{
  return(mus_write_1(tfd, beg, end, chans, bufs, NULL, false));
}

int mus_file_write_file(int tfd, int beg, int end, int chans, mus_sample_t **bufs)
{
  return(mus_write_1(tfd, beg, end, chans, bufs, NULL, false));
}

int mus_file_write_buffer(int charbuf_data_format, int beg, int end, int chans, mus_sample_t **bufs, char *charbuf, bool clipped)
{
  return(mus_write_1(charbuf_data_format, beg, end, chans, bufs, charbuf, clipped));
}


/* for CLM */
void mus_reset_io_c(void) 
{
  io_fd_size = 0;
  io_fds = NULL;
  clipping_default = false;
  prescaler_default = 1.0;
}


#if (!HAVE_STRDUP)
/* this taken from libit-0.7 */
char *strdup (const char *str)
{
  char *newstr;
  newstr = (char *)malloc(strlen(str) + 1);
  if (newstr) strcpy(newstr, str);
  return(newstr);
}
#endif

#if (!HAVE_FILENO)
/* this is needed by XtAppAddInput */
int fileno(FILE *fp)
{
  if (fp == stdout)
    return(0);
  else
    {
      if (fp == stdin)
	return(1);
    }
  return(2);
}
#endif

static int sndlib_strlen(const char *str)
{
  /* strlen(NULL) -> seg fault! */
  if ((str) && (*str)) return(strlen(str));
  return(0);
}

char *mus_getcwd(void)
{
  int i, path_max = 0;
  char *pwd = NULL, *res = NULL;
#if HAVE_PATHCONF
  path_max = pathconf("/", _PC_PATH_MAX);
#endif
  if (path_max < 1024)
    {
#if defined(PATH_MAX)
      path_max = PATH_MAX;
#endif
      if (path_max < 1024) 
	path_max = 1024;
    }
#if HAVE_GETCWD
  for (i = path_max;; i *= 2)
    {
      if (pwd) FREE(pwd);
      pwd = (char *)CALLOC(i, sizeof(char));
      res = getcwd(pwd, i);
      if (res) break;    /* NULL is returned if failure, but what about success? should I check errno=ERANGE? */
    }
#else
#if HAVE_GETWD
  pwd = (char *)CALLOC(path_max, sizeof(char));
  getwd(pwd);
#endif
#endif
  return(pwd);
}

char *mus_expand_filename(const char *filename)
{
  /* fill out under-specified library pathnames etc */
#if defined(MUS_WINDOZE) && (!(defined(__CYGWIN__)))
  return(strdup(filename));
#else
  char *file_name_buf = NULL;
  char *tok = NULL, *orig = NULL;
  int i, j = 0, len = 0;
  if ((filename) && (*filename)) 
    len = strlen(filename); 
  else return(NULL);
  if (len == 0) return(NULL);
  orig = strdup(filename);
  tok = orig;
  /* get rid of "//" */
  for (i = 0; i < len - 1; i++)
    {
      if ((tok[i] == '/') && 
	  (tok[i + 1] == '/')) 
	j = i + 1;
    }
  if (j > 0)
    {
      for (i = 0; j < len; i++, j++) 
	tok[i] = tok[j];
      tok[i] ='\0';
    }
  /* get rid of "~/" at start */
  if (tok[0] != '/')
    {
      char *home = NULL;
      if ((tok[0] == '~') && (home = getenv("HOME")))
	{
	  file_name_buf = (char *)CALLOC(len + sndlib_strlen(home) + 8, sizeof(char));
	  strcpy(file_name_buf, home);
	  strcat(file_name_buf, ++tok);
	}
      else
	{
	  char *pwd;
	  pwd = mus_getcwd();
	  file_name_buf = (char *)CALLOC(len + sndlib_strlen(pwd) + 8, sizeof(char));
	  strcpy(file_name_buf, pwd);
	  FREE(pwd);
	  strcat(file_name_buf, "/");
	  if (tok[0])
	    strcat(file_name_buf, tok);
	}
    }
  else 
    {
      file_name_buf = (char *)CALLOC(len + 8, sizeof(char));
      strcpy(file_name_buf, tok);
    }
  /* get rid of "/../" and "/./" also "/." at end */
  {
    int slash_at = -1;
    bool found_one = true;
    while (found_one)
      {
	found_one = false;
	len = strlen(file_name_buf);
	for (i = 0; i < len - 3; i++)
	  if (file_name_buf[i] == '/')
	    {
	      if ((file_name_buf[i + 1] == '.') &&
		  (file_name_buf[i + 2] == '.') &&
		  (file_name_buf[i + 3] == '/'))
		{
		  i += 4;
		  for (j = slash_at + 1; i < len; i++, j++)
		    file_name_buf[j] = file_name_buf[i];
		  file_name_buf[j] = '\0';
		  found_one = true;
		  break;
		}
	      else
		{
		  if ((file_name_buf[i + 1] == '.') &&
		      (file_name_buf[i + 2] == '/'))
		    {
		      for (j = i + 3, i = i + 1; j < len; i++, j++)
			file_name_buf[i] = file_name_buf[j];
		      file_name_buf[i] = '\0';
		      found_one = true;
		    }
		  else slash_at = i;
		}
	    }
      }
    len = strlen(file_name_buf);
    if ((len > 1) &&
	(file_name_buf[len - 1] == '.') &&
	(file_name_buf[len - 2] == '/'))
      file_name_buf[len - 1] = '\0';
  }
  free(orig);
  return(file_name_buf);
#endif
}


void mus_snprintf(char *buffer, int buffer_len, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
#if HAVE_VSNPRINTF
  vsnprintf(buffer, buffer_len, format, ap);
#else
  vsprintf(buffer, format, ap);
#endif
  va_end(ap);
}

#define MUS_FORMAT_STRING_MAX 1024

char *mus_format(const char *format, ...)
{
  /* caller should free result */
  char *buf = NULL, *rtn = NULL;
  va_list ap;
  buf = (char *)CALLOC(MUS_FORMAT_STRING_MAX, sizeof(char));
  va_start(ap, format);
#if HAVE_VSNPRINTF
  vsnprintf(buf, MUS_FORMAT_STRING_MAX, format, ap);
#else
  vsprintf(buf, format, ap);
#endif
  va_end(ap);
#if MUS_DEBUGGING
  rtn = copy_string(buf);
#else
  rtn = strdup(buf);
#endif
  FREE(buf);
  return(rtn);
}

Float mus_fclamp(Float lo, Float val, Float hi) 
{
  if (val > hi) 
    return(hi); 
  else 
    if (val < lo) 
      return(lo); 
    else return(val);
}

int mus_iclamp(int lo, int val, int hi) 
{
  if (val > hi) 
    return(hi); 
  else 
    if (val < lo) 
      return(lo); 
    else return(val);
}

off_t mus_oclamp(off_t lo, off_t val, off_t hi) 
{
  if (val > hi) 
    return(hi); 
  else 
    if (val < lo) 
      return(lo); 
    else return(val);
}
