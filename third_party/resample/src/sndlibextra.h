#include <endian.h>      /* so that sndlib.h will get host byte-order right */
#include <sndlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* for creating, opening and closing files for sndlib I/O */
int sndlib_create(char *, int, int, int, int, char *);
int sndlib_open_read(char *);
int sndlib_open_write(char *);
int sndlib_close(int, int, int, int, int);

/* for reading and writing headers */
int sndlib_read_header(int);
int sndlib_write_header(int, int, int, int, int, int, char *, int *);
int sndlib_set_header_data_size(int, int, int);

/* helper functions */
int **sndlib_allocate_buffers(int, int);
void sndlib_free_buffers(int **, int);


/* some handy macros */

#define IS_FLOAT_FORMAT(format) (                        \
              (format) == snd_32_float                   \
           || (format) == snd_32_float_little_endian     )

#define NOT_A_SOUND_FILE(header_type) (                  \
              (header_type) == MUS_UNSUPPORTED    \
           || (header_type) == MUS_RAW            )

/* The header types that sndlib can write, as of sndlib-5.5. */
#define WRITEABLE_HEADER_TYPE(type) (                    \
               (type) == MUS_AIFF                 \
           ||  (type) == MUS_NEXT                 \
           ||  (type) == MUS_RIFF                 \
           ||  (type) == MUS_IRCAM                )

#define INVALID_DATA_FORMAT(format) ((format) < 1)


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

