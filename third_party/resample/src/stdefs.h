/* stdefs.h */
#ifndef _STDEFS_H
#define _STDEFS_H

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef PI
#define PI (3.14159265358979232846)
#endif

#ifndef PI2
#define PI2 (6.28318530717958465692)
#endif

#define D2R (0.01745329348)          /* (2*pi)/360 */
#define R2D (57.29577951)            /* 360/(2*pi) */

#ifndef MAX
#define MAX(x,y) ((x)>(y) ?(x):(y))
#endif
#ifndef MIN
#define MIN(x,y) ((x)<(y) ?(x):(y))
#endif

#ifndef ABS
#define ABS(x)   ((x)<0   ?(-(x)):(x))
#endif

#ifndef SGN
#define SGN(x)   ((x)<0   ?(-1):((x)==0?(0):(1)))
#endif

typedef char           BOOL;
typedef short          HWORD;
typedef unsigned short UHWORD;
typedef int            WORD;
typedef unsigned int   UWORD;

#define MUS_SAMPLE_TYPE_TO_HWORD(x) ((HWORD)((x)>>(MUS_SAMPLE_BITS-16)))
#define HWORD_TO_MUS_SAMPLE_TYPE(x) ((mus_sample_t)((x)<<(MUS_SAMPLE_BITS-16)))

#define MAX_HWORD (32767)
#define MIN_HWORD (-32768)

#ifdef DEBUG
#define INLINE
#else
#ifndef INLINE
#define INLINE inline
#endif
#endif

#endif /* _STDEFS_H */
