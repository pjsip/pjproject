/*
 * FILE: resample.c
 * Sampling-rate-conversion main program (command line usage)
 */

static char resampleVersion[] 
    = "\n\tresample version 1.9 (Feb. 1, 2006 - jos@ccrma.stanford.edu)\n\n\
Copyright 1982-2006 by Julius Smith.\n\
This is free software. See the Lesser GNU Public License (LGPL) for copying conditions.\n\
There is NO warranty;  not even for MERCHANTABILITY or FITNESS FOR A\n\
PARTICULAR PURPOSE.\n\n\
";
    
#define USAGE "\
\n\
USAGE: One of the following:\n\
\n\
      resample -to srate [-noFilterInterp] [-linearSignalInterp] [-f filterFile] [-terse] inputSoundFile outputSoundFile\n\
      resample -by factor [options as above] inputSoundFile outputSoundFile\n\
      resample -version\n\
\n\
Options can be abbreviated.\n\n\
Report bugs to <bug-resample@w3k.org>.\n\n\
"

#include "filterkit.h"
#include "resample.h"
#include "sndlibextra.h"
    
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

static int trace = 1;		/* controls verbosity of output */

static char comment[256] = "";

static void fail(char *s)
{
    fprintf(stderr,"\n*** resample: %s \n",s); /* Display error message  */
    fprintf(stderr,USAGE);
    exit(1);			/* Exit, indicating error */
}

static void fails(char *s, char *s2)
{
    printf("resample: ");           /* Display error message  */
    printf(s,s2);
    printf("\n\n");
    exit(1);                        /* Exit, indicating error */
}


int main(int argc, char *argv[])
{
    double factor = -2.0;	/* factor = Sndout/Sndin */
    double newsrate=0;
    BOOL interpFilt = TRUE;	/* TRUE means interpolate filter coeffs */
    BOOL largeFilter = FALSE;	/* TRUE means use 65-tap FIR filter */
    BOOL linearInterp = FALSE;	/* TRUE => no filter, linearly interpolate */
    BOOL knowFactor = FALSE;	/* Used to detect insufficient command-line spec */
    int inCount, outCount, outCountReal;
    int infd, outfd, insrate, nChans, result;
    int inType, inFormat;
    int outType, outFormat;

    struct stat statbuf;
    char *insfname, *outsfname, *argv0;
    char filterFile[512] = "";

    if (argc == 1) {
	fprintf(stderr, USAGE);
	exit(1);
    }

    argv0 = argv[0];
    while (--argc && **(++argv)=='-') {
	++(argv[0]); /* skip over '-' */
	switch (*argv[0]) {
	case 'a':			       /* -aaaQuality (old name) */
	case 'e':			       /* -expensive */
	    largeFilter = TRUE;
	    if (trace)
	      printf("Choosing higher quality filter.\n");
	    break;
	case 'b':			       /* -by factor */
	    if (--argc)
	      sscanf(*(++argv),"%lf",&factor);
	    if (trace)
	      printf("Sampling-rate conversion factor set to %f.\n",factor);
	    knowFactor = TRUE;
	    break;
	case 'f':  			       /* -filter filterFile */
	    if (--argc)
		strcpy(filterFile, *++argv);
	    else
		fail("Need to specify filter file name");
	    if (trace)
	      printf("Filter file set to %s.\n",filterFile);
	    break;
	case 'l':				/* -linearInpterpolation */
	    linearInterp = TRUE;
	    if (trace)
	      printf("Using linear instead of bandlimited interpolation\n");
	    break;
	case 'n':			       /* -noFilterInterpolation */
	    interpFilt = FALSE;
	    if (trace)
	      printf("Filter-table interpolation disabled.\n");
	    break;
	case 't':
	    if (*(argv[0]+1) == 'e') { 		/* -terse */
		trace = 0;
		break;
	    }
	    if (--argc)				/* -to srate */
	      sscanf(*(++argv),"%lf",&newsrate);
	    if (trace)
	      printf("Target sampling-rate set to %f.\n",newsrate);
	    knowFactor = TRUE;
	    break;
	case 'v':			       /* -version */
	    printf(resampleVersion);
	    if (argc == 1)
		exit(0);
	    break;
	default:
	    fprintf(stderr,"Unknown switch -%s\n",argv[0]);
	    fprintf(stderr,USAGE);
	    exit(1);
	}
    }
    
    if (!knowFactor)
      fail("Must specify sampling-rate conversion factor via '-to' or '-by' option");

    if (argc < 1)
      fail("Need to specify input soundfile");
    insfname = *argv;

    if (argc < 2) {
	fprintf(stderr, USAGE);
	exit(1);
    }
    else
      outsfname = *++argv;

    /* Test whether output file name exists. If so, bail... */
    result = stat(outsfname, &statbuf);
    if (result != -1)
	fails("\"%s\" already exists", outsfname);
    if (errno != ENOENT)
	fails("Error creating output file (%s)", strerror(errno));

    if (trace)
	printf("Writing output to \"%s\".\n", outsfname);

    /* Open input file and gather info from its header */
    // resample-1.8: infd = sndlib_open_read(insfname);
    infd = mus_sound_open_input(insfname);
    if (infd == -1)
	fails("Could not open input file \"%s\"", insfname);
    if (NOT_A_SOUND_FILE(mus_header_type()))
	fails("\"%s\" is probably not a sound file.", insfname);
    nChans = mus_header_chans();
    insrate = mus_header_srate();
    inCount = mus_header_samples();
    inType = mus_header_type(); /* header type (i.e. aiff, wave, etc)  (see sndlib.h) */
    inFormat = mus_header_format(); /* data format (see sndlib.h) */
    inCount /= nChans;           /* to get sample frames */

/* 
 * Set output soundfile format.
 * See sndlib.h for supported types (e.g., MUS_RIFF).
 */
    outType = inType;
    outFormat = inFormat;

    /* Compute sampling rate conversion factor, if not specified. */
    if (newsrate>0) {
	if (factor>0)
	    fprintf(stderr, "Command-line sampling-rate conversion factor "
		  "ignored ... the '-to' option overrides the '-by' option.\n");
	factor = newsrate / (double)insrate;
	if (trace)
	    printf("Sampling rate conversion factor set to %f\n",factor);
    }

    if (factor < 0) {
	factor = -factor;
	factor = GetDouble("Sampling-rate conversion factor",factor,"");
    }

    if (newsrate <= 0)
      newsrate = (int)((double)insrate * factor + 0.5); /* round */
    
    sprintf(comment,"%s -by %f %s%s%s%s%s%s%s %s",argv0,factor,
	    (largeFilter?"-expensiveFilter ":""),
	    (strcmp(filterFile,"")==0?"":"-f "),
	    (strcmp(filterFile,"")==0?"":filterFile),
	    (strcmp(filterFile,"")==0?"":" "),
	    (linearInterp? "-linearSigInterp ":""),
	    (interpFilt? "":"-noFilterInterp "),
	    insfname, outsfname);

    outfd = sndlib_create(outsfname, inType, inFormat, newsrate, nChans, comment);
    if (outfd == -1)
	fails("Could not create output file \"%s\"", outsfname);
    outCount = (int)(factor * (double)inCount + 0.5);       /* output frames */

    printf("\nStarting Conversion\n");
    outCountReal = resample(factor, infd, outfd, inCount, outCount, nChans,
			    interpFilt, linearInterp, largeFilter, filterFile);

    if (outCountReal <= 0)
      fail("Conversion factor out of range");

    if (trace && (outCount != outCountReal))
      fprintf(stderr,
	      "outCount = %d, outCountReal = %d\n",outCount,outCountReal);

    // resample-1.8: sndlib_close(infd, FALSE, 0, 0, 0);
    mus_file_close(infd);

    /* Output samps already written; just update header and close file. */
    // resample-1.8: if (sndlib_close(outfd, 1, outType, outFormat, outCountReal * nChans))
    // resample-1.8:  fails("Error closing output file (%s)", strerror(errno));
    mus_file_close(outfd);
    int sound_bytes = outCountReal * nChans * mus_bytes_per_sample(outFormat);
    mus_header_change_data_size(outsfname, inType, sound_bytes);

    printf("\nConversion Finished:  %d output samples.\n\n",outCount);

    return(0);
}
