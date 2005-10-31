/* $Header: /pjproject/pjmedia/src/test/rtp_test.c 4     3/12/05 4:21p Bennylp $ */
#include <pjmedia/rtp.h>
#include <stdio.h>

int rtp_test()
{
    pj_rtp_session rtp;
    FILE *fhnd = fopen("RTP.DAT", "wb");
    const void *rtphdr;
    int hdrlen;

    if (!fhnd)
	return -1;

    pj_rtp_session_init (&rtp, 4, 0x12345678);
    pj_rtp_encode_rtp (&rtp, 4, 0, 0, 160, &rtphdr, &hdrlen);
    fwrite (rtphdr, hdrlen, 1, fhnd);
    fclose(fhnd);
    return 0;
}
