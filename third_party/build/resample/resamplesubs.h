
int 
  SrcLinear(short X[], short Y[], double factor, unsigned int *Time, unsigned short Nx);

int SrcUp(short X[], short Y[], double factor, unsigned int *Time,
          unsigned short Nx, unsigned short Nwing, unsigned short LpScl,
          short Imp[], short ImpD[], char Interp);

int SrcUD(short X[], short Y[], double factor, unsigned int *Time,
          unsigned short Nx, unsigned short Nwing, unsigned short LpScl,
          short Imp[], short ImpD[], char Interp);

extern unsigned resample_LARGE_FILTER_NMULT;
extern unsigned resample_LARGE_FILTER_NWING;
extern unsigned resample_LARGE_FILTER_SCALE;
extern short* resample_LARGE_FILTER_IMP;
extern short* resample_LARGE_FILTER_IMPD;

extern unsigned resample_SMALL_FILTER_NMULT;
extern unsigned resample_SMALL_FILTER_NWING;
extern unsigned resample_SMALL_FILTER_SCALE;
extern short* resample_SMALL_FILTER_IMP;
extern short* resample_SMALL_FILTER_IMPD;
