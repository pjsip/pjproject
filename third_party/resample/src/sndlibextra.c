/* sndlibextra.c

   An interface to the low-level sndlib functions contained in sndlib/headers.c
   and sndlib/io.c, designed for use by the resample program.
   (sndlibextra.c is a stripped-down version of sndlibsupport.c in RTcmix.) 

   - John Gibson (jgg9c@virginia.edu)
*/
#include <assert.h>
#include <string.h>
#include "sndlibextra.h"

/* #define NDEBUG */     /* define to disable asserts */

/* ---------------------------------------------------- sndlib_create --- */
/* Creates a new file and writes a header with the given characteristics.
   <type> is a sndlib constant for header type (e.g. MUS_AIFF).
   <format> is a sndlib constant for sound data format (e.g. snd_16_linear).
   (These constants are defined in sndlib.h.) Caller is responsible for
   checking that the header type and data format are compatible, and that
   the header type is one that sndlib can write (is WRITEABLE_HEADER_TYPE()).
   Writes no comment.

   NOTE: This will truncate an existing file with <sfname>, so check first!

   On success, returns a standard file descriptor, and leaves the file
   position pointer at the end of the header.
   On failure, returns -1. Caller can check errno then.
*/
int
sndlib_create(char *sfname, /* file name */
	      int type, /* file type, e.g. AIFF */
	      int format, /* data format, e.g., MU-LAW */
	      int srate, /* sampling rate in Hz */
	      int chans, /* 1 for mono, 2 for stereo */
	      char *comment)
{
   int  fd, loc;
   assert(sfname != NULL && strlen(sfname) <= FILENAME_MAX);
   mus_header_initialize(); // make sure relevant parts of sndlib are initialized
   //resample-1.8: sndlib_write_header(fd, 0, type, format, srate, chans, comment, &loc)
   //resample-1.8: mus_file_open_descriptors(fd, format, mus_header_data_format_to_bytes_per_sample(),loc);
   // int mus_file_open_descriptors(int tfd, const char *arg, int df, int ds, off_t dl, int dc, int dt);
   fd = mus_sound_open_output(sfname, srate, chans, format, type, "created by resample");
   return fd;
}

/* ------------------------------------------ sndlib_allocate_buffers --- */
/* Allocate the multi-dimensional array required by sndlib I/O functions.
   Returns an array of <nchans> arrays of <nframes> integers. The memory
   is cleared. If the return value is NULL, check errno.
*/
int **
sndlib_allocate_buffers(int nchans, int nframes)
{
   int **bufs, n;

   assert(nchans > 0 && nframes > 0);

   bufs = (int **)calloc(nchans, sizeof(int *));
   if (bufs == NULL)
      return NULL;

   for (n = 0; n < nchans; n++) {
      bufs[n] = (int *)calloc(nframes, sizeof(int));
      if (bufs[n] == NULL) {
         sndlib_free_buffers(bufs, nchans);
         return NULL;
      }
   }

   return bufs;
}


/* ---------------------------------------------- sndlib_free_buffers --- */
/* Free the multi-dimensional array <bufs> with <nchans> elements.
*/
void
sndlib_free_buffers(int **bufs, int nchans)
{
   int n;

   assert(bufs != NULL);

   for (n = 0; n < nchans; n++)
      if (bufs[n])
         free(bufs[n]);
   free(bufs);
}
