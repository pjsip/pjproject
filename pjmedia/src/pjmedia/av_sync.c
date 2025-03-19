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

/* Enable/disable trace */
#if 0
#  define TRACE_(x) PJ_LOG(5, x)
#else
#  define TRACE_(x)
#endif

/* AV sync media */
struct pjmedia_av_sync_media
{
    PJ_DECL_LIST_MEMBER(struct pjmedia_av_sync_media);

    pjmedia_av_sync         *av_sync;           /* The AV sync instance     */
    pjmedia_av_sync_media_setting setting;      /* Media settings           */

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
    pjmedia_av_sync_setting  setting;

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
    PJ_LOG(4, (avs->setting.name, "%s destroyed", avs->setting.name));
    pj_pool_release(avs->pool);
}


/* Get default values for synchronizer settings. */
PJ_DEF(void) pjmedia_av_sync_setting_default(
                                pjmedia_av_sync_setting *setting)
{
    pj_bzero(setting, sizeof(*setting));
}


/* Get default values for media settings. */
PJ_DEF(void) pjmedia_av_sync_media_setting_default(
                                pjmedia_av_sync_media_setting* setting)
{
    pj_bzero(setting, sizeof(*setting));
}


/* Create media synchronizer. */
PJ_DEF(pj_status_t) pjmedia_av_sync_create(
                                pj_pool_t *pool_,
                                const pjmedia_av_sync_setting *setting,
                                pjmedia_av_sync **av_sync)
{
    pj_pool_t* pool = NULL;
    pjmedia_av_sync* avs = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool_ && av_sync && setting, PJ_EINVAL);

    pool = pj_pool_create(pool_->factory, "avsync%p", 512, 512, NULL);
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
    avs->setting = *setting;
    if (setting->name) {
        pj_size_t len = PJ_MIN(PJ_MAX_OBJ_NAME,
                               pj_ansi_strlen(setting->name)+1);
        avs->setting.name = pj_pool_zalloc(avs->pool, len);
        pj_ansi_snprintf(avs->setting.name, len, "%s", setting->name);
    } else {
        avs->setting.name = pool->obj_name;
    }

    status = pj_grp_lock_create_w_handler(pool, NULL, avs, &avs_on_destroy,
                                          &avs->grp_lock);
    if (status != PJ_SUCCESS)
        goto on_error;

    pj_grp_lock_add_ref(avs->grp_lock);
    pj_list_init(&avs->media_list);
    pj_list_init(&avs->free_media_list);

    PJ_LOG(4, (avs->setting.name, "%s created", avs->setting.name));
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
    PJ_LOG(4, (avs->setting.name, "%s destroy requested",
               avs->setting.name));
    pj_grp_lock_dec_ref(avs->grp_lock);
}


/* Reset synchronization states. */
PJ_DEF(pj_status_t) pjmedia_av_sync_reset(pjmedia_av_sync *avs)
{
    pjmedia_av_sync_media* m;
    PJ_ASSERT_RETURN(avs, PJ_EINVAL);

    pj_grp_lock_acquire(avs->grp_lock);
    avs->max_ntp.u64 = 0;
    avs->slowdown_req_ms = 0;

    m = avs->media_list.next;
    while (m != &avs->media_list) {
        m->is_ref_set = PJ_FALSE;
        m->last_ntp.u64 = 0;
        m->last_adj_delay_req = 0;
        m->adj_delay_req_cnt = 0;
        m->smooth_diff = 0;
        m = m->next;
    }
    pj_grp_lock_release(avs->grp_lock);
    return PJ_SUCCESS;
}


/* Add media to synchronizer. */
PJ_DEF(pj_status_t) pjmedia_av_sync_add_media(
                                pjmedia_av_sync *avs,
                                const pjmedia_av_sync_media_setting *setting,
                                pjmedia_av_sync_media **media)
{
    pjmedia_av_sync_media* m;
    pj_status_t status = PJ_SUCCESS;
    char* m_name;

    PJ_ASSERT_RETURN(avs && media && setting, PJ_EINVAL);

    pj_grp_lock_acquire(avs->grp_lock);

    /* Get media from free list, if any, otherwise allocate a new one */
    if (!pj_list_empty(&avs->free_media_list)) {
        m = avs->free_media_list.next;
        pj_list_erase(m);
        m_name = m->setting.name;
    } else {
        m = PJ_POOL_ZALLOC_T(avs->pool, pjmedia_av_sync_media);
        m_name = pj_pool_zalloc(avs->pool, PJ_MAX_OBJ_NAME);
        if (!m || !m_name) {
            status = PJ_ENOMEM;
            goto on_return;
        }
    }

    m->av_sync = avs;
    m->setting = *setting;
    if (setting->name) {
        pj_ansi_snprintf(m_name, PJ_MAX_OBJ_NAME, "%s", setting->name);
    } else {
        pj_ansi_snprintf(m_name, PJ_MAX_OBJ_NAME, "avs_med_%d",
                         ++avs->last_idx);
    }
    m->setting.name = m_name;

    pj_list_push_back(&avs->media_list, m);
    pj_grp_lock_add_ref(avs->grp_lock);

    *media = m;
    PJ_LOG(4, (avs->setting.name, "Added media %s, clock rate=%d",
               m->setting.name, m->setting.clock_rate));

on_return:
    pj_grp_lock_release(avs->grp_lock);
    return status;
}


/* Remove media from synchronizer. */
PJ_DEF(pj_status_t) pjmedia_av_sync_del_media(
                                pjmedia_av_sync *avs,
                                pjmedia_av_sync_media *media)
{
    PJ_ASSERT_RETURN(media, PJ_EINVAL);
    PJ_ASSERT_RETURN(!avs || media->av_sync == avs, PJ_EINVAL);

    if (!avs)
        avs = media->av_sync;

    pj_grp_lock_acquire(avs->grp_lock);
    pj_list_erase(media);

    /* Zero some fields */
    media->is_ref_set = PJ_FALSE;
    media->last_adj_delay_req = 0;
    media->adj_delay_req_cnt = 0;
    media->smooth_diff = 0;

    pj_list_push_back(&avs->free_media_list, media);
    pj_grp_lock_release(avs->grp_lock);

    PJ_LOG(4, (avs->setting.name, "Removed media %s", media->setting.name));
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
    TRACE_((media->av_sync->setting.name, "%s updates ref ntp=%u ts=%u",
            media->setting.name, ntp->u64, ts->u64));

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_av_sync_update_pts(
                                pjmedia_av_sync_media *media,
                                const pj_timestamp *pts,
                                pj_int32_t *adjust_delay)
{
    pjmedia_av_sync *avs;
    pj_int32_t diff;
    pj_timestamp max_ntp;

    PJ_ASSERT_RETURN(media && media->av_sync && pts, PJ_EINVAL);

    /* Reset the adjustment delay */
    if (adjust_delay)
        *adjust_delay = 0;

    /* Make sure we have a reference */
    if (!media->is_ref_set)
        return PJ_EINVALIDOP;

    diff = pj_timestamp_diff32(&media->ref_ts, pts);

    /* Only process if pts is increasing */
    if (diff <= 0)
        return PJ_ETOOSMALL;

    avs = media->av_sync;
    TRACE_((avs->setting.name, "%s updates pts=%u",
            media->setting.name, pts->u64));

    /* Update last presentation time */
    media->last_ntp = media->ref_ntp;
    ntp_add_ts(&media->last_ntp, diff, media->setting.clock_rate);

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
            TRACE_((avs->setting.name,
                    "%s is requested to slow down by %dms",
                    media->setting.name, media->last_adj_delay_req));
            if (adjust_delay)
                *adjust_delay = media->last_adj_delay_req;

            return PJ_SUCCESS;
        }
    } else {
        /* Not the fastest. */
        pj_timestamp ntp_diff = max_ntp;
        unsigned ms_diff, ms_req;

        /* First, check the lag from the fastest. */
        pj_sub_timestamp(&ntp_diff, &media->last_ntp);
        ms_diff = ntp_to_ms(&ntp_diff);

        /* For streaming, smoothen (apply weight of 9 for current lag),
         * and round down the lag to the nearest 10.
         */
        if (avs->setting.is_streaming) {
            ms_diff = ((ms_diff + 9 * media->smooth_diff) / 100) * 10;
            media->smooth_diff = ms_diff;
        }

        /* The lag is tolerable, just return 0 */
        if (ms_diff <= PJMEDIA_AVSYNC_MAX_TOLERABLE_LAG_MSEC) {
            if (media->last_adj_delay_req) {
                TRACE_((avs->setting.name,
                        "%s lag looks good now=%ums",
                        media->setting.name, ms_diff));
            }
            /* Reset the request delay & counter */
            media->adj_delay_req_cnt = 0;
            media->last_adj_delay_req = 0;

            return PJ_SUCCESS;
        }

        /* Check if any speed-up request has been done before */
        if (media->last_adj_delay_req) {
            /* Check if request number has reached limit */
            if (media->adj_delay_req_cnt>=PJMEDIA_AVSYNC_MAX_SPEEDUP_REQ_CNT)
            {

                /* After several requests this media still cannot catch up,
                 * signal the synchronizer to slow down the fastest media.
                 *
                 * For streaming mode, request slow down 3/4 of required to
                 * prevent possible delay increase on all media.
                 */
                ms_req = ms_diff;
                if (avs->setting.is_streaming)
                    ms_req = ms_req * 3/4;

                if (avs->slowdown_req_ms < ms_req)
                    avs->slowdown_req_ms = ms_req;

                TRACE_((avs->setting.name,
                        "%s request limit has been reached, requesting "
                        "the fastest media to slow down by %ums",
                        media->setting.name, avs->slowdown_req_ms));

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
                               (PJMEDIA_AVSYNC_MAX_SPEEDUP_REQ_CNT -
                                media->adj_delay_req_cnt + 1);
                if (progress >= min_expected) {
                    /* Yes, let's just request again and wait */
                    TRACE_((avs->setting.name,
                            "%s speeds up in progress, current lag=%ums",
                            media->setting.name, ms_diff));
                }
            }
        } else {
            /* First request to speed up */
            media->adj_delay_req_cnt = 0;
        }

        /* Request the media to speed up & increment the counter.
         *
         * For streaming mode, request speed-up 4/3 of required to
         * prevent possible delay increase on all media.
         */
        ms_req = ms_diff;
        if (avs->setting.is_streaming)
            ms_req = ms_req * 4/3;

        media->last_adj_delay_req = -(pj_int32_t)ms_req;
        media->adj_delay_req_cnt++;

        TRACE_((avs->setting.name,
                "%s is requested to speed up #%d by %dms",
                media->setting.name, media->adj_delay_req_cnt,
                -media->last_adj_delay_req));

        if (adjust_delay)
            *adjust_delay = media->last_adj_delay_req;

        return PJ_SUCCESS;
    }

    return PJ_SUCCESS;
}
