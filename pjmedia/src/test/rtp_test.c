/* $Id$
 *
 */
/* 
 * PJMEDIA - Multimedia over IP Stack 
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
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
