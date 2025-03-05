/*
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia/av_sync.h>
#include <pjmedia/endpoint.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/list.h>
#include <pj/os.h>

/* Maximum tolerable delay from the fastest/earliest media, in milliseconds.
 * When delay is higher than this setting, some actions will be required,
 * i.e: speed-up or slow-down media playback.
 */
#define MAX_DELAY_MS                    50

 /* Maximum number of request to a media to speeding up its delay to match
  * to the fastest media, before requesting the fastest media to slow down.
  */
#define MAX_DELAY_REQ_CNT               10

/* Enable/disable trace */
#if 1
#  define TRACE_(x) PJ_LOG(1, x)
#else
#  define TRACE_(x)
#endif

/* AV sync media */
struct pjmedia_av_sync_media
{
    PJ_DECL_LIST_MEMBER(struct pjmedia_av_sync_media);

    pjmedia_av_sync         *av_sync;           /* The AV sync instance     */
    unsigned                 clock_rate;        /* Media clock rate         */
    char                    *name;              /* Internal name,
                                                   for logging purpose      */

    /* Reference timestamp */
    pj_bool_t                is_ref_set;        /* Has reference been set?  */
    pj_timestamp             ref_ts;            /* Ref ts, in sample units  */
    pj_timestamp             ref_ntp;           /* Ref ts, in NTP units     */

    /* Last presentation timestamp */
    pj_timestamp             last_ntp;          /* Last PTS, in NTP units   */
    pj_int32_t               smooth_diff;

    /* Delay adjustment requested to this media */
    pj_int32_t               last_adj_delay_req; /* Last requested delay    */
    unsigned                 adj_delay_req_cnt;  /* Request counter         */
};


/* AV sync */
struct pjmedia_av_sync
{
    pj_pool_t               *pool;
    pjmedia_av_sync_media    media_list;
    pjmedia_av_sync_media    free_media_list;
    pj_grp_lock_t           *grp_lock;
    unsigned                 last_idx;

    /* Maximum NTP time of all media */
    pj_timestamp             max_ntp;

    /* Some media cannot catch up, request for slow down, in milliseconds */
    unsigned                 slowdown_req_ms;
};


static void ntp_add_ts(pj_timestamp* ntp, unsigned ts, unsigned clock_rate)
{
    pj_timestamp ts_diff;

    ts_diff.u64 = ((pj_uint64_t)ts << 32) / clock_rate;
    pj_add_timestamp(ntp, &ts_diff);
}

static unsigned ntp_to_ms(const pj_timestamp* ntp)
{
    pj_uint64_t ms;

    ms = ntp->u32.hi * 1000 + (((pj_uint64_t)ntp->u32.lo * 1000) >> 32);
    return (unsigned)ms;
}


/* AV sync destroy handler. */
static void avs_on_destroy(void* arg)
{
    pjmedia_av_sync* avs = (pjmedia_av_sync*)arg;
    TRACE_((avs->pool->obj_name, "%s destroyed", avs->pool->obj_name));
    pj_pool_release(avs->pool);
}

/* Get default setting for media. */
PJ_DEF(void) pjmedia_av_sync_media_setting_default(
                                pjmedia_av_sync_media_setting* setting)
{
    pj_bzero(setting, sizeof(*setting));
}

/* Create media synchronizer. */
PJ_DEF(pj_status_t) pjmedia_av_sync_create(
                                pjmedia_endpt *endpt,
                                const void *setting,
                                pjmedia_av_sync **av_sync)
{
    pj_pool_t* pool = NULL;
    pjmedia_av_sync* avs = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && av_sync, PJ_EINVAL);
    PJ_UNUSED_ARG(setting);

    pool = pjmedia_endpt_create_pool(endpt, "avsync%p", 512, 512);
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    avs = PJ_POOL_ZALLOC_T(pool, pjmedia_av_sync);
    if (!avs) {
        status = PJ_ENOMEM;
        goto on_error;
    }
    avs->pool = pool;

    status = pj_grp_lock_create_w_handler(pool, NULL, avs, &avs_on_destroy,
                                          &avs->grp_lock);
    if (status != PJ_SUCCESS)
        goto on_error;

    pj_grp_lock_add_ref(avs->grp_lock);
    pj_list_init(&avs->media_list);
    pj_list_init(&avs->free_media_list);

    TRACE_((avs->pool->obj_name, "%s created", avs->pool->obj_name));
    *av_sync = avs;
    return PJ_SUCCESS;

on_error:
    if (pool)
        pj_pool_release(pool);

    return status;
}


/* Destroy media synchronizer. */
PJ_DEF(void) pjmedia_av_sync_destroy(pjmedia_av_sync* avs)
{
    PJ_ASSERT_ON_FAIL(avs, return);
    pj_grp_lock_dec_ref(avs->grp_lock);
}


/* Add media to synchronizer. */
PJ_DEF(pj_status_t) pjmedia_av_sync_add_media(
                                pjmedia_av_sync *avs,
                                const pjmedia_av_sync_media_setting *setting,
                                pjmedia_av_sync_media **media)
{
    pjmedia_av_sync_media* m;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(avs && media, PJ_EINVAL);
    PJ_UNUSED_ARG(setting);

    pj_grp_lock_acquire(avs->grp_lock);

    /* Get media from free list, if any, otherwise allocate a new one */
    if (!pj_list_empty(&avs->free_media_list)) {
        m = avs->free_media_list.next;
        pj_list_erase(m);
    } else {
        char *m_name;
        m = PJ_POOL_ZALLOC_T(avs->pool, pjmedia_av_sync_media);
        m_name = pj_pool_zalloc(avs->pool, PJ_MAX_OBJ_NAME);
        if (!m || !m_name) {
            status = PJ_ENOMEM;
            goto on_return;
        }
        m->name = m_name;
    }

    m->av_sync = avs;
    m->clock_rate = setting->clock_rate;
    if (setting->name) {
        pj_ansi_strncpy(m->name, setting->name, PJ_MAX_OBJ_NAME);
    } else {
        pj_ansi_snprintf(m->name, PJ_MAX_OBJ_NAME, "avs_med_%d",
                         ++avs->last_idx);
    }

    pj_list_push_back(&avs->media_list, m);
    pj_grp_lock_add_ref(avs->grp_lock);

    *media = m;
    TRACE_((avs->pool->obj_name, "Added media %s", m->name));

on_return:
    pj_grp_lock_release(avs->grp_lock);
    return status;
}


/* Remove media from synchronizer. */
PJ_DEF(pj_status_t) pjmedia_av_sync_del_media(
                                pjmedia_av_sync *avs,
                                pjmedia_av_sync_media *media)
{
    PJ_ASSERT_RETURN(avs && media, PJ_EINVAL);
    PJ_ASSERT_RETURN(media->av_sync == avs, PJ_EINVAL);

    pj_grp_lock_acquire(avs->grp_lock);
    pj_list_erase(media);

    /* Zero some fields */
    media->is_ref_set = PJ_FALSE;
    media->last_adj_delay_req = 0;
    media->adj_delay_req_cnt = 0;
    media->smooth_diff = 0;

    pj_list_push_back(&avs->free_media_list, media);
    pj_grp_lock_release(avs->grp_lock);

    TRACE_((avs->pool->obj_name, "Removed media %s", media->name));
    pj_grp_lock_dec_ref(avs->grp_lock);

    return PJ_SUCCESS;
}


/* Update synchronizer about the last timestamp reference of the specified
 * media.
 */
PJ_DEF(pj_status_t) pjmedia_av_sync_update_ref(
                                pjmedia_av_sync_media* media,
                                const pj_timestamp* ntp,
                                const pj_timestamp* ts)
{
    PJ_ASSERT_RETURN(media && ntp && ts, PJ_EINVAL);

    media->ref_ntp = *ntp;
    media->ref_ts  = *ts;
    media->is_ref_set = PJ_TRUE;

    return PJ_SUCCESS;
}


PJ_DEF(pj_int32_t) pjmedia_av_sync_update_pts(
                                pjmedia_av_sync_media *media,
                                const pj_timestamp *pts)
{
    pjmedia_av_sync *avs;
    pj_int32_t diff;
    pj_timestamp max_ntp;

    PJ_ASSERT_RETURN(media && media->av_sync && pts, PJ_EINVAL);

    /* Make sure we have a reference */
    if (!media->is_ref_set)
        return 0;

    avs = media->av_sync;
    diff = pj_timestamp_diff32(&media->ref_ts, pts);

    /* Only process if:
     * - pts is increasing, and
     * - not jumping too far (< one minutes).
     */
    if (diff <= 0 || diff >= (int)media->clock_rate*60) {
        /* Reset reference */
        //media->is_ref_set = PJ_FALSE;
        return 0;
    }

    /* Update last presentation time */
    media->last_ntp = media->ref_ntp;
    ntp_add_ts(&media->last_ntp, diff, media->clock_rate);

    /* Get NTP timestamp of the earliest media */
    pj_grp_lock_acquire(avs->grp_lock);
    max_ntp = avs->max_ntp;
    pj_grp_lock_release(avs->grp_lock);

    /* Check if this media is the fastest/earliest */
    if (pj_cmp_timestamp(&media->last_ntp, &max_ntp) > 0) {
        /* Yes, it is the fastest, update the max timestamp */
        pj_grp_lock_acquire(avs->grp_lock);
        avs->max_ntp = media->last_ntp;
        pj_grp_lock_release(avs->grp_lock);

        /* Check if there is any request to slow down */
        if (avs->slowdown_req_ms) {
            media->last_adj_delay_req = avs->slowdown_req_ms;
            media->adj_delay_req_cnt = 0;
            avs->slowdown_req_ms = 0;
            TRACE_((avs->pool->obj_name, "%s is requested to slow down %dms",
                    media->name, media->last_adj_delay_req));
            return media->last_adj_delay_req;
        }
    } else {
        /* Not the fastest. */
        pj_timestamp ntp_diff = max_ntp;
        unsigned ms_diff;

        /* First, check the delay from the fastest. */
        pj_sub_timestamp(&ntp_diff, &media->last_ntp);
        ms_diff = ntp_to_ms(&ntp_diff);

        /* Smoothen and round down the delay */
        ms_diff = ((ms_diff + 19 * media->smooth_diff) / 200) * 10;
        media->smooth_diff = ms_diff;

        /* The delay is tolerable, just return 0 */
        if (ms_diff <= MAX_DELAY_MS) {
            if (media->last_adj_delay_req) {
                TRACE_((avs->pool->obj_name,
                        "%s speeds up completed, delay looks good=%ums",
                        media->name, ms_diff));
            }
            /* Reset the request delay & counter */
            media->adj_delay_req_cnt = 0;
            media->last_adj_delay_req = 0;
            return 0;
        }

        /* Check if any speed-up request has been done before */
        if (media->last_adj_delay_req) {
            /* Check if request number has reached limit */
            if (media->adj_delay_req_cnt >= MAX_DELAY_REQ_CNT) {
                /* After several requests this media still cannot catch up,
                 * signal the synchronizer to slow down the fastest media.
                 */
                if (avs->slowdown_req_ms < ms_diff)
                    avs->slowdown_req_ms = ms_diff;

                TRACE_((avs->pool->obj_name,
                        "%s request limit has been reached, requesting "
                        "the fastest media to slow down by %ums",
                        media->name, avs->slowdown_req_ms));

                /* Reset the request counter.
                 * And still keep requesting for speed up, shouldn't we?
                 */
                media->adj_delay_req_cnt = 0;
            } else {
                pj_int32_t progress, min_expected;

                /* Check if the previous delay request has shown some
                 * progress.
                 */
                progress = (-media->last_adj_delay_req) - ms_diff;
                min_expected = -media->last_adj_delay_req /
                           (MAX_DELAY_REQ_CNT - media->adj_delay_req_cnt + 1);
                if (progress >= min_expected) {
                    /* Yes, let's just request again and wait */
                    TRACE_((avs->pool->obj_name,
                            "%s speeds up in progress, current delay=%ums",
                            media->name, ms_diff));
                }
            }
        } else {
            /* First request to speed up */
            media->adj_delay_req_cnt = 0;
        }

        /* Request the media to speed up & increment the counter */
        media->last_adj_delay_req = -(pj_int32_t)ms_diff;
        media->adj_delay_req_cnt++;

        TRACE_((avs->pool->obj_name,
                "%s is requested to speed up #%d %ums",
                media->name, media->adj_delay_req_cnt, ms_diff));

        return media->last_adj_delay_req;
    }

    return 0;
}
