/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */


/*
 * DO NOT COMPILE THIS FILE ON ITS OWN!
 *
 * This file is included by siprtp.c to implement the reporting capability
 * to a separate file, so that user can implement different reporting
 * functionality (such as writing to XML file).
 */

static const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}


static void print_call(int call_index)
{
    struct call *call = &app.call[call_index];
    int len;
    pjsip_inv_session *inv = call->inv;
    pjsip_dialog *dlg = inv->dlg;
    struct media_stream *audio = &call->media[0];
    char userinfo[128];
    char duration[80], last_update[80];
    char bps[16], ipbps[16], packets[16], bytes[16], ipbytes[16];
    pj_time_val now;

    pj_gettimeofday(&now);

    if (app.report_filename)
	puts(app.report_filename);

    /* Print duration */
    if (inv->state >= PJSIP_INV_STATE_CONFIRMED && call->connect_time.sec) {

	PJ_TIME_VAL_SUB(now, call->connect_time);

	sprintf(duration, " [duration: %02ld:%02ld:%02ld.%03ld]",
		now.sec / 3600,
		(now.sec % 3600) / 60,
		(now.sec % 60),
		now.msec);

    } else {
	duration[0] = '\0';
    }



    /* Call number and state */
    printf("Call #%d: %s%s\n", call_index, pjsip_inv_state_name(inv->state), 
			       duration);



    /* Call identification */
    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 1)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';

    printf("   %s\n", userinfo);


    if (call->inv == NULL || call->inv->state < PJSIP_INV_STATE_CONFIRMED ||
	call->connect_time.sec == 0) 
    {
	return;
    }


    /* Signaling quality */
    {
	char pdd[64], connectdelay[64];
	pj_time_val t;

	if (call->response_time.sec) {
	    t = call->response_time;
	    PJ_TIME_VAL_SUB(t, call->start_time);
	    sprintf(pdd, "got 1st response in %ld ms", PJ_TIME_VAL_MSEC(t));
	} else {
	    pdd[0] = '\0';
	}

	if (call->connect_time.sec) {
	    t = call->connect_time;
	    PJ_TIME_VAL_SUB(t, call->start_time);
	    sprintf(connectdelay, ", connected after: %ld ms", 
		    PJ_TIME_VAL_MSEC(t));
	} else {
	    connectdelay[0] = '\0';
	}

	printf("   Signaling quality: %s%s\n", pdd, connectdelay);
    }


    printf("   Stream #0: audio %.*s@%dHz, %dms/frame, %sB/s (%sB/s +IP hdr)\n",
   	(int)audio->si.fmt.encoding_name.slen,
	audio->si.fmt.encoding_name.ptr,
	audio->clock_rate,
	audio->samples_per_frame * 1000 / audio->clock_rate,
	good_number(bps, audio->bytes_per_frame * audio->clock_rate / audio->samples_per_frame),
	good_number(ipbps, (audio->bytes_per_frame+32) * audio->clock_rate / audio->samples_per_frame));

    if (audio->rtcp.stat.rx.update_cnt == 0)
	strcpy(last_update, "never");
    else {
	pj_gettimeofday(&now);
	PJ_TIME_VAL_SUB(now, audio->rtcp.stat.rx.update);
	sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		now.sec / 3600,
		(now.sec % 3600) / 60,
		now.sec % 60,
		now.msec);
    }

    printf("              RX stat last update: %s\n"
	   "                 total %s packets %sB received (%sB +IP hdr)%s\n"
	   "                 pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
	   "                       (msec)    min     avg     max     last\n"
	   "                 loss period: %7.3f %7.3f %7.3f %7.3f%s\n"
	   "                 jitter     : %7.3f %7.3f %7.3f %7.3f%s\n",
	   last_update,
	   good_number(packets, audio->rtcp.stat.rx.pkt),
	   good_number(bytes, audio->rtcp.stat.rx.bytes),
	   good_number(ipbytes, audio->rtcp.stat.rx.bytes + audio->rtcp.stat.rx.pkt * 32),
	   "",
	   audio->rtcp.stat.rx.loss,
	   audio->rtcp.stat.rx.loss * 100.0 / (audio->rtcp.stat.rx.pkt + audio->rtcp.stat.rx.loss),
	   audio->rtcp.stat.rx.dup, 
	   audio->rtcp.stat.rx.dup * 100.0 / (audio->rtcp.stat.rx.pkt + audio->rtcp.stat.rx.dup),
	   audio->rtcp.stat.rx.reorder, 
	   audio->rtcp.stat.rx.reorder * 100.0 / (audio->rtcp.stat.rx.pkt + audio->rtcp.stat.rx.reorder),
	   "",
	   audio->rtcp.stat.rx.loss_period.min / 1000.0, 
	   audio->rtcp.stat.rx.loss_period.avg / 1000.0, 
	   audio->rtcp.stat.rx.loss_period.max / 1000.0,
	   audio->rtcp.stat.rx.loss_period.last / 1000.0,
	   "",
	   audio->rtcp.stat.rx.jitter.min / 1000.0,
	   audio->rtcp.stat.rx.jitter.avg / 1000.0,
	   audio->rtcp.stat.rx.jitter.max / 1000.0,
	   audio->rtcp.stat.rx.jitter.last / 1000.0,
	   ""
	   );


    if (audio->rtcp.stat.tx.update_cnt == 0)
	strcpy(last_update, "never");
    else {
	pj_gettimeofday(&now);
	PJ_TIME_VAL_SUB(now, audio->rtcp.stat.tx.update);
	sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		now.sec / 3600,
		(now.sec % 3600) / 60,
		now.sec % 60,
		now.msec);
    }

    printf("              TX stat last update: %s\n"
	   "                 total %s packets %sB sent (%sB +IP hdr)%s\n"
	   "                 pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
	   "                       (msec)    min     avg     max     last\n"
	   "                 loss period: %7.3f %7.3f %7.3f %7.3f%s\n"
	   "                 jitter     : %7.3f %7.3f %7.3f %7.3f%s\n",
	   last_update,
	   good_number(packets, audio->rtcp.stat.tx.pkt),
	   good_number(bytes, audio->rtcp.stat.tx.bytes),
	   good_number(ipbytes, audio->rtcp.stat.tx.bytes + audio->rtcp.stat.tx.pkt * 32),
	   "",
	   audio->rtcp.stat.tx.loss,
	   audio->rtcp.stat.tx.loss * 100.0 / (audio->rtcp.stat.tx.pkt + audio->rtcp.stat.tx.loss),
	   audio->rtcp.stat.tx.dup, 
	   audio->rtcp.stat.tx.dup * 100.0 / (audio->rtcp.stat.tx.pkt + audio->rtcp.stat.tx.dup),
	   audio->rtcp.stat.tx.reorder, 
	   audio->rtcp.stat.tx.reorder * 100.0 / (audio->rtcp.stat.tx.pkt + audio->rtcp.stat.tx.reorder),
	   "",
	   audio->rtcp.stat.tx.loss_period.min / 1000.0, 
	   audio->rtcp.stat.tx.loss_period.avg / 1000.0, 
	   audio->rtcp.stat.tx.loss_period.max / 1000.0,
	   audio->rtcp.stat.tx.loss_period.last / 1000.0,
	   "",
	   audio->rtcp.stat.tx.jitter.min / 1000.0,
	   audio->rtcp.stat.tx.jitter.avg / 1000.0,
	   audio->rtcp.stat.tx.jitter.max / 1000.0,
	   audio->rtcp.stat.tx.jitter.last / 1000.0,
	   ""
	   );


    printf("             RTT delay      : %7.3f %7.3f %7.3f %7.3f%s\n", 
	   audio->rtcp.stat.rtt.min / 1000.0,
	   audio->rtcp.stat.rtt.avg / 1000.0,
	   audio->rtcp.stat.rtt.max / 1000.0,
	   audio->rtcp.stat.rtt.last / 1000.0,
	   ""
	   );

}

