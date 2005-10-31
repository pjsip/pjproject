/* $Header: /pjproject/pjmedia/src/pjmedia/session.c 10    6/14/05 12:54a Bennylp $ */
#include <pjmedia/session.h>
#include <pj/log.h>
#include <pj/os.h> 
#include <pj/pool.h>
#include <pj/string.h>

typedef struct pj_media_stream_desc
{
    pj_media_stream_info    info;
    pj_media_stream_t	   *enc_stream, *dec_stream;
} pj_media_stream_desc;

struct pj_media_session_t
{
    pj_pool_t		   *pool;
    pj_med_mgr_t	   *mediamgr;
    unsigned		    stream_cnt;
    pj_media_stream_desc   *stream_desc[PJSDP_MAX_MEDIA];
};

#define THIS_FILE		"session.c"

#define PJ_MEDIA_SESSION_SIZE	(48*1024)
#define PJ_MEDIA_SESSION_INC	1024

static const pj_str_t ID_AUDIO = { "audio", 5};
static const pj_str_t ID_VIDEO = { "video", 5};
static const pj_str_t ID_IN = { "IN", 2 };
static const pj_str_t ID_IP4 = { "IP4", 3};
static const pj_str_t ID_RTP_AVP = { "RTP/AVP", 7 };
static const pj_str_t ID_SDP_NAME = { "pjmedia", 7 };

static void session_init (pj_media_session_t *ses)
{
    pj_memset (ses, 0, sizeof(pj_media_session_t));
}


/**
 * Create new session offering.
 */
PJ_DEF(pj_media_session_t*) 
pj_media_session_create (pj_med_mgr_t *mgr, const pj_media_sock_info *sock_info)
{
    pj_pool_factory *pf;
    pj_pool_t *pool;
    pj_media_session_t *session;
    pj_media_stream_desc *sd;
    unsigned i, codec_cnt;
    pj_codec_mgr *cm;
    const pj_codec_id *codecs[PJSDP_MAX_FMT];

    pf = pj_med_mgr_get_pool_factory(mgr);

    pool = pj_pool_create( pf, "session", PJ_MEDIA_SESSION_SIZE, PJ_MEDIA_SESSION_INC, NULL);
    if (!pool)
	return NULL;

    session = pj_pool_alloc(pool, sizeof(pj_media_session_t));
    if (!session)
	return NULL;

    session_init (session);

    session->pool = pool;
    session->mediamgr = mgr;

    /* Create first stream  */
    sd = pj_pool_calloc (pool, 1, sizeof(pj_media_stream_desc));
    if (!sd)
	return NULL;

    sd->info.type = ID_AUDIO;
    sd->info.dir = PJ_MEDIA_DIR_ENCODING_DECODING;
    sd->info.transport = ID_RTP_AVP;
    pj_memcpy(&sd->info.sock_info, sock_info, sizeof(*sock_info));

    /* Enum audio codecs. */
    sd->info.fmt_cnt = 0;
    cm = pj_med_mgr_get_codec_mgr (mgr);
    codec_cnt = pj_codec_mgr_enum_codecs(cm, PJSDP_MAX_FMT, codecs);
    if (codec_cnt > PJSDP_MAX_FMT) codec_cnt = PJSDP_MAX_FMT;
    for (i=0; i<codec_cnt; ++i) {
	if (codecs[i]->type != PJ_MEDIA_TYPE_AUDIO)
	    continue;

	sd->info.fmt[sd->info.fmt_cnt].pt = codecs[i]->pt;
	sd->info.fmt[sd->info.fmt_cnt].sample_rate = codecs[i]->sample_rate;
	pj_strdup (pool, &sd->info.fmt[sd->info.fmt_cnt].encoding_name, &codecs[i]->encoding_name);
	++sd->info.fmt_cnt;
    }

    session->stream_desc[session->stream_cnt++] = sd;

    return session;
}

static int sdp_check (const pjsdp_session_desc *sdp)
{
    int has_conn = 0;
    unsigned i;

    if (sdp->conn)
	has_conn = 1;

    if (sdp->media_count == 0) {
	PJ_LOG(4,(THIS_FILE, "SDP check failed: no media stream definition"));
	return -1;
    }

    for (i=0; i<sdp->media_count; ++i) {
	pjsdp_media_desc *m = sdp->media[i];

	if (!m) {
	    pj_assert(0);
	    return -1;
	}

	if (m->desc.fmt_count == 0) {
	    PJ_LOG(4,(THIS_FILE, "SDP check failed: no format listed in media stream"));
	    return -1;
	}

	if (!has_conn && m->conn == NULL) {
	    PJ_LOG(4,(THIS_FILE, "SDP check failed: no connection information for media"));
	    return -1;
	}
    }

    return 0;
}

/*
 * Create local stream definition that matches SDP received from peer.
 */
static pj_media_stream_desc* 
create_stream_from_sdp (pj_pool_t *pool, pj_med_mgr_t *mgr, const pjsdp_conn_info *conn,
			const pjsdp_media_desc *m, const pj_media_sock_info *sock_info)
{
    pj_media_stream_desc *sd;

    sd = pj_pool_calloc (pool, 1, sizeof(pj_media_stream_desc));
    if (!sd) {
	PJ_LOG(2,(THIS_FILE, "No memory to allocate stream descriptor"));
	return NULL;
    }

    if (pj_stricmp(&conn->net_type, &ID_IN)==0 && 
	pj_stricmp(&conn->addr_type, &ID_IP4)==0 &&
	pj_stricmp(&m->desc.media, &ID_AUDIO)==0 &&
	pj_stricmp(&m->desc.transport, &ID_RTP_AVP) == 0) 
    {
	/*
	 * Got audio stream.
	 */
	unsigned i, codec_cnt;
	pj_codec_mgr *cm;
	const pj_codec_id *codecs[PJSDP_MAX_FMT];

	sd->info.type = ID_AUDIO;
	sd->info.transport = ID_RTP_AVP;
	pj_memcpy(&sd->info.sock_info, sock_info, sizeof(*sock_info));
	sd->info.rem_port = m->desc.port;
	pj_strdup (pool, &sd->info.rem_addr, &conn->addr);

	/* Enum audio codecs. */
	sd->info.fmt_cnt = 0;
	cm = pj_med_mgr_get_codec_mgr (mgr);
	codec_cnt = pj_codec_mgr_enum_codecs (cm, PJSDP_MAX_FMT, codecs);
	if (codec_cnt > PJSDP_MAX_FMT) codec_cnt = PJSDP_MAX_FMT;

	/* Find just one codec which we can support. */
	for (i=0; i<m->desc.fmt_count && sd->info.fmt_cnt == 0; ++i) {
	    unsigned j, fmt_i;

	    /* For static payload, just match payload type. */
	    /* Else match clock rate and encoding name. */
	    fmt_i = pj_strtoul(&m->desc.fmt[i]);
	    if (fmt_i < PJ_RTP_PT_DYNAMIC) {
		for (j=0; j<codec_cnt; ++j) {
		    if (codecs[j]->pt == fmt_i) {
			sd->info.fmt_cnt = 1;
			sd->info.fmt[0].type = PJ_MEDIA_TYPE_AUDIO;
			sd->info.fmt[0].pt = codecs[j]->pt;
			sd->info.fmt[0].sample_rate = codecs[j]->sample_rate;
			pj_strdup (pool, &sd->info.fmt[0].encoding_name, &codecs[j]->encoding_name);
			break;
		    }
		}
	    } else {

		/* Find the rtpmap for the payload type. */
		const pjsdp_rtpmap_attr *rtpmap = pjsdp_media_desc_find_rtpmap (m, fmt_i);

		/* Don't accept the media if no rtpmap for dynamic PT. */
		if (rtpmap == NULL) {
		    PJ_LOG(4,(THIS_FILE, "SDP: No rtpmap found for payload id %d", m->desc.fmt[i]));
		    continue;
		}

		/* Check whether we can take this codec. */
		for (j=0; j<codec_cnt; ++j) {
		    if (rtpmap->clock_rate == codecs[j]->sample_rate && 
			pj_stricmp(&rtpmap->encoding_name, &codecs[j]->encoding_name) == 0)
		    {
			sd->info.fmt_cnt = 1;
			sd->info.fmt[0].type = PJ_MEDIA_TYPE_AUDIO;
			sd->info.fmt[0].pt = codecs[j]->pt;
			sd->info.fmt[0].sample_rate = codecs[j]->sample_rate;
			sd->info.fmt[0].encoding_name = codecs[j]->encoding_name;
			break;
		    }
		}
	    }
	}

	/* Match codec and direction. */
	if (sd->info.fmt_cnt == 0 || m->desc.port == 0 || 
	    pjsdp_media_desc_has_attr(m, PJSDP_ATTR_INACTIVE)) 
	{
	    sd->info.dir = PJ_MEDIA_DIR_NONE;
	}
	else if (pjsdp_media_desc_has_attr(m, PJSDP_ATTR_RECV_ONLY)) {
	    sd->info.dir = PJ_MEDIA_DIR_ENCODING;
	}
	else if (pjsdp_media_desc_has_attr(m, PJSDP_ATTR_SEND_ONLY)) {
	    sd->info.dir = PJ_MEDIA_DIR_DECODING;
	}
	else {
	    sd->info.dir = PJ_MEDIA_DIR_ENCODING_DECODING;
	}

    } else {
	/* Unsupported media stream. */
	unsigned fmt_num;
	const pjsdp_rtpmap_attr *rtpmap = NULL;

	pj_strdup(pool, &sd->info.type, &m->desc.media);
	pj_strdup(pool, &sd->info.transport, &m->desc.transport);
	pj_memset(&sd->info.sock_info, 0, sizeof(*sock_info));
	pj_strdup (pool, &sd->info.rem_addr, &conn->addr);
	sd->info.rem_port = m->desc.port;

	/* Just put one format and rtpmap, so that we don't have to make 
	 * special exception when we convert this stream to SDP.
	 */

	/* Find the rtpmap for the payload type. */
	fmt_num = pj_strtoul(&m->desc.fmt[0]);
	rtpmap = pjsdp_media_desc_find_rtpmap (m, fmt_num);

	sd->info.fmt_cnt = 1;
	if (pj_stricmp(&m->desc.media, &ID_VIDEO)==0) {
	    sd->info.fmt[0].type = PJ_MEDIA_TYPE_VIDEO;
	    sd->info.fmt[0].pt = fmt_num;
	    if (rtpmap) {
		pj_strdup (pool, &sd->info.fmt[0].encoding_name, 
			   &rtpmap->encoding_name);
		sd->info.fmt[0].sample_rate = rtpmap->clock_rate;
	    }
	} else {
	    sd->info.fmt[0].type = PJ_MEDIA_TYPE_UNKNOWN;
	    pj_strdup(pool, &sd->info.fmt[0].encoding_name, &m->desc.fmt[0]);
	}
	
	sd->info.dir = PJ_MEDIA_DIR_NONE;
    }

    return sd;
}

/**
 * Create new session based on peer's offering.
 */
PJ_DEF(pj_media_session_t*) 
pj_media_session_create_from_sdp (pj_med_mgr_t *mgr, const pjsdp_session_desc *sdp,
				  const pj_media_sock_info *sock_info)
{
    pj_pool_factory *pf;
    pj_pool_t *pool;
    pj_media_session_t *session;
    unsigned i;

    if (sdp_check(sdp) != 0)
	return NULL;

    pf = pj_med_mgr_get_pool_factory(mgr);
    pool = pj_pool_create( pf, "session", PJ_MEDIA_SESSION_SIZE, PJ_MEDIA_SESSION_INC, NULL);
    if (!pool)
	return NULL;

    session = pj_pool_alloc(pool, sizeof(pj_media_session_t));
    if (!session) {
	PJ_LOG(3,(THIS_FILE, "No memory to create media session descriptor"));
	pj_pool_release (pool);
	return NULL;
    }

    session_init (session);

    session->pool = pool;
    session->mediamgr = mgr;

    /* Enumerate each media stream and create our peer. */
    for (i=0; i<sdp->media_count; ++i) {
	const pjsdp_conn_info *conn;
	const pjsdp_media_desc *m;
	pj_media_stream_desc *sd;

	m = sdp->media[i];
	conn = m->conn ? m->conn : sdp->conn;

	/*
	 * Bug:
	 *  the sock_info below is used by more than one 'm' lines
	 */
	PJ_TODO(SUPPORT_MORE_THAN_ONE_SDP_M_LINES)

	sd = create_stream_from_sdp (pool, mgr, conn, m, sock_info);
	pj_assert (sd);

	session->stream_desc[session->stream_cnt++] = sd;
    }

    return session;
}

/**
 * Duplicate session. The new session is inactive.
 */
PJ_DEF(pj_media_session_t*)
pj_media_session_clone (const pj_media_session_t *rhs)
{
    pj_pool_factory *pf;
    pj_pool_t *pool;
    pj_media_session_t *session;
    unsigned i;

    pf = pj_med_mgr_get_pool_factory(rhs->mediamgr);
    pool = pj_pool_create( pf, "session", PJ_MEDIA_SESSION_SIZE, PJ_MEDIA_SESSION_INC, NULL);
    if (!pool) {
	return NULL;
    }

    session = pj_pool_alloc (pool, sizeof(*session));
    if (!session) {
	PJ_LOG(3,(THIS_FILE, "No memory to create media session descriptor"));
	pj_pool_release (pool);
	return NULL;
    }

    session->pool = pool;
    session->mediamgr = rhs->mediamgr;
    session->stream_cnt = rhs->stream_cnt;

    for (i=0; i<rhs->stream_cnt; ++i) {
	pj_media_stream_desc *sd1 = pj_pool_alloc (session->pool, sizeof(pj_media_stream_desc));
	const pj_media_stream_desc *sd2 = rhs->stream_desc[i];

	if (!sd1) {
	    PJ_LOG(3,(THIS_FILE, "No memory to create media stream descriptor"));
	    pj_pool_release (pool);
	    return NULL;
	}

	session->stream_desc[i] = sd1;
	sd1->enc_stream = sd1->dec_stream = NULL;
	pj_strdup (pool, &sd1->info.type, &sd2->info.type);
	sd1->info.dir = sd2->info.dir;
	pj_strdup (pool, &sd1->info.transport, &sd2->info.transport);
	pj_memcpy(&sd1->info.sock_info, &sd2->info.sock_info, sizeof(pj_media_sock_info));
	pj_strdup (pool, &sd1->info.rem_addr, &sd2->info.rem_addr);
	sd1->info.rem_port = sd2->info.rem_port;
	sd1->info.fmt_cnt = sd2->info.fmt_cnt;
	pj_memcpy (sd1->info.fmt, sd2->info.fmt, sizeof(sd2->info.fmt));
    }

    return session;
}

/**
 * Create SDP description from the session.
 */
PJ_DEF(pjsdp_session_desc*)
pj_media_session_create_sdp (const pj_media_session_t *session, pj_pool_t *pool,
			     pj_bool_t only_first_fmt)
{
    pjsdp_session_desc *sdp;
    pj_time_val tv;
    unsigned i;
    pj_media_sock_info *c_addr = NULL;

    if (session->stream_cnt == 0) {
	pj_assert(0);
	return NULL;
    }

    sdp = pj_pool_calloc (pool, 1, sizeof(pjsdp_session_desc));
    if (!sdp) {
	PJ_LOG(3,(THIS_FILE, "No memory to allocate SDP session descriptor"));
	return NULL;
    }

    pj_gettimeofday(&tv);

    sdp->origin.user = pj_str("-");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = ID_IN;
    sdp->origin.addr_type = ID_IP4;
    sdp->origin.addr = *pj_gethostname();

    sdp->name = ID_SDP_NAME;

    /* If all media addresses are the same, then put the connection
     * info in the session level, otherwise put it in media stream
     * level.
     */
    for (i=0; i<session->stream_cnt; ++i) {
	if (c_addr == NULL) {
	    c_addr = &session->stream_desc[i]->info.sock_info;
	} else if (c_addr->rtp_addr_name.sin_addr.s_addr != session->stream_desc[i]->info.sock_info.rtp_addr_name.sin_addr.s_addr)
	{
	    c_addr = NULL;
	    break;
	}
    }

    if (c_addr) {
	/* All addresses are the same, put connection info in session level. */
	sdp->conn = pj_pool_alloc (pool, sizeof(pjsdp_conn_info));
	if (!sdp->conn) {
	    PJ_LOG(2,(THIS_FILE, "No memory to allocate SDP connection info"));
	    return NULL;
	}

	sdp->conn->net_type = ID_IN;
	sdp->conn->addr_type = ID_IP4;
	pj_strdup2 (pool, &sdp->conn->addr, pj_inet_ntoa(c_addr->rtp_addr_name.sin_addr));
    }

    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;

    /* Create each media. */
    sdp->media_count = 0;
    for (i=0; i<session->stream_cnt; ++i) {
	const pj_media_stream_desc *sd = session->stream_desc[i];
	pjsdp_media_desc *m;
	unsigned j;
	unsigned fmt_cnt;
	pjsdp_attr *attr;

	m = pj_pool_calloc (pool, 1, sizeof(pjsdp_media_desc));
	if (!m) {
	    PJ_LOG(3,(THIS_FILE, "No memory to allocate SDP media stream descriptor"));
	    return NULL;
	}

	sdp->media[sdp->media_count++] = m;

	pj_strdup (pool, &m->desc.media, &sd->info.type);
	m->desc.port = pj_ntohs(sd->info.sock_info.rtp_addr_name.sin_port);
	m->desc.port_count = 1;
	pj_strdup (pool, &m->desc.transport, &sd->info.transport);

	/* Add format and rtpmap for each codec. */
	m->desc.fmt_count = 0;
	m->attr_count = 0;
	fmt_cnt = sd->info.fmt_cnt;
	if (fmt_cnt > 0 && only_first_fmt)
	    fmt_cnt = 1;
	for (j=0; j<fmt_cnt; ++j) {
	    pjsdp_rtpmap_attr *rtpmap;
	    pj_str_t *fmt = &m->desc.fmt[m->desc.fmt_count++];

	    if (sd->info.fmt[j].type==PJ_MEDIA_TYPE_UNKNOWN) {
		pj_strdup(pool, fmt, &sd->info.fmt[j].encoding_name);
	    } else {
		fmt->ptr = pj_pool_alloc(pool, 8);
		fmt->slen = pj_utoa(sd->info.fmt[j].pt, fmt->ptr);

		rtpmap = pj_pool_calloc(pool, 1, sizeof(pjsdp_rtpmap_attr));
		if (rtpmap) {
		    m->attr[m->attr_count++] = (pjsdp_attr*)rtpmap;
		    rtpmap->type = PJSDP_ATTR_RTPMAP;
		    rtpmap->payload_type = sd->info.fmt[j].pt;
		    rtpmap->clock_rate = sd->info.fmt[j].sample_rate;
		    pj_strdup (pool, &rtpmap->encoding_name, &sd->info.fmt[j].encoding_name);
		} else {
		    PJ_LOG(3,(THIS_FILE, "No memory to allocate SDP rtpmap descriptor"));
		}
	    }
	}

	/* If we don't have connection info in session level, create one. */
	if (sdp->conn == NULL) {
	    m->conn = pj_pool_alloc (pool, sizeof(pjsdp_conn_info));
	    if (m->conn) {
		m->conn->net_type = ID_IN;
		m->conn->addr_type = ID_IP4;
		pj_strdup2 (pool, &m->conn->addr, pj_inet_ntoa(sd->info.sock_info.rtp_addr_name.sin_addr));
	    } else {
		PJ_LOG(3,(THIS_FILE, "No memory to allocate SDP media connection info"));
		return NULL;
	    }
	}

	/* Add additional attribute to the media stream. */
	attr = pj_pool_alloc(pool, sizeof(pjsdp_attr));
	if (!attr) {
	    PJ_LOG(3,(THIS_FILE, "No memory to allocate SDP attribute"));
	    return NULL;
	}
	m->attr[m->attr_count++] = attr;

	switch (sd->info.dir) {
	case PJ_MEDIA_DIR_NONE:
	    attr->type = PJSDP_ATTR_INACTIVE;
	    break;
	case PJ_MEDIA_DIR_ENCODING:
	    attr->type = PJSDP_ATTR_SEND_ONLY;
	    break;
	case PJ_MEDIA_DIR_DECODING:
	    attr->type = PJSDP_ATTR_RECV_ONLY;
	    break;
	case PJ_MEDIA_DIR_ENCODING_DECODING:
	    attr->type = PJSDP_ATTR_SEND_RECV;
	    break;
	}
    }

    return sdp;
}

/**
 * Update session with SDP answer from peer.
 */
PJ_DEF(pj_status_t)
pj_media_session_update (pj_media_session_t *session, 
			 const pjsdp_session_desc *sdp)
{
    unsigned i;
    unsigned count;

    /* Check SDP */
    if (sdp_check (sdp) != 0) {
	return -1;
    }

    /* If the media stream count doesn't match, only update one. */
    if (session->stream_cnt != sdp->media_count) {
	PJ_LOG(3,(THIS_FILE, "pj_media_session_update : "
		 "SDP media count mismatch! (rmt=%d, lcl=%d)",
		 sdp->media_count, session->stream_cnt));
	count = (session->stream_cnt < sdp->media_count) ?
	    session->stream_cnt : sdp->media_count;
    } else {
	count = session->stream_cnt;
    }

    for (i=0; i<count; ++i) {
	pj_media_stream_desc *sd = session->stream_desc[i];
	const pjsdp_media_desc *m = sdp->media[i];
	const pjsdp_conn_info *conn;
	unsigned j;

	/* Check that the session is not active. */
	pj_assert (sd->enc_stream == NULL && sd->dec_stream == NULL);

	conn = m->conn ? m->conn : sdp->conn;
	pj_assert(conn);

	/* Update remote address. */
	sd->info.rem_port = m->desc.port;
	pj_strdup (session->pool, &sd->info.rem_addr, &conn->addr);

	/* Select one active codec according to what peer wants. */
	for (j=0; j<sd->info.fmt_cnt; ++j) {
	    unsigned fmt_0 = pj_strtoul(&m->desc.fmt[0]);
	    if (sd->info.fmt[j].pt == fmt_0) {
		pj_codec_id temp;

		/* Put active format to the front. */
		if (j == 0)
		    break;

		pj_memcpy(&temp, &sd->info.fmt[0], sizeof(temp));
		pj_memcpy(&sd->info.fmt[0], &sd->info.fmt[j], sizeof(temp));
		pj_memcpy(&sd->info.fmt[j], &temp, sizeof(temp));
		break;
	    }
	}

	if (j == sd->info.fmt_cnt) {
	    /* Peer has answered SDP with new codec, which doesn't exist
	     * in the offer!
	     * Mute this media.
	     */
	    PJ_LOG(3,(THIS_FILE, "Peer has answered SDP with new codec!"));
	    sd->info.dir = PJ_MEDIA_DIR_NONE;
	    continue;
	}

	/* Check direction. */
	if (m->desc.port == 0 || pjsdp_media_desc_has_attr(m, PJSDP_ATTR_INACTIVE)) {
	    sd->info.dir = PJ_MEDIA_DIR_NONE;
	}
	else if (pjsdp_media_desc_has_attr(m, PJSDP_ATTR_RECV_ONLY)) {
	    sd->info.dir = PJ_MEDIA_DIR_ENCODING;
	}
	else if (pjsdp_media_desc_has_attr(m, PJSDP_ATTR_SEND_ONLY)) {
	    sd->info.dir = PJ_MEDIA_DIR_DECODING;
	}
	else {
	    sd->info.dir = PJ_MEDIA_DIR_ENCODING_DECODING;
	}
    }

    return 0;
}

/**
 * Enumerate media streams in the session.
 */
PJ_DEF(unsigned)
pj_media_session_enum_streams (const pj_media_session_t *session, 
			       unsigned count, const pj_media_stream_info *info[])
{
    unsigned i;

    if (count > session->stream_cnt)
	count = session->stream_cnt;

    for (i=0; i<count; ++i) {
	info[i] = &session->stream_desc[i]->info;
    }

    return session->stream_cnt;
}

/**
 * Get statistics
 */
PJ_DEF(pj_status_t)
pj_media_session_get_stat (const pj_media_session_t *session, unsigned index,
			   pj_media_stream_stat *tx_stat,
			   pj_media_stream_stat *rx_stat)
{
    pj_media_stream_desc *sd;
    int stat_cnt = 0;

    if (index >= session->stream_cnt) {
	pj_assert(0);
	return -1;
    }
    
    sd = session->stream_desc[index];

    if (sd->enc_stream && tx_stat) {
	pj_media_stream_get_stat (sd->enc_stream, tx_stat);
	++stat_cnt;
    } else if (tx_stat) {
	pj_memset (tx_stat, 0, sizeof(*tx_stat));
    }

    if (sd->dec_stream && rx_stat) {
	pj_media_stream_get_stat (sd->dec_stream, rx_stat);
	++stat_cnt;
    } else if (rx_stat) {
	pj_memset (rx_stat, 0, sizeof(*rx_stat));
    }

    return stat_cnt ? 0 : -1;
}

/**
 * Modify stream, only when stream is inactive.
 */
PJ_DEF(pj_status_t)
pj_media_session_modify_stream (pj_media_session_t *session, unsigned index,
				unsigned modify_flag, const pj_media_stream_info *info)
{
    pj_media_stream_desc *sd;

    if (index >= session->stream_cnt) {
	pj_assert(0);
	return -1;
    }
    
    sd = session->stream_desc[index];

    if (sd->enc_stream || sd->dec_stream) {
	pj_assert(0);
	return -1;
    }

    if (modify_flag & PJ_MEDIA_STREAM_MODIFY_DIR) {
	sd->info.dir = info->dir;
    }

    return 0;
}

/**
 * Activate media session.
 */
PJ_DEF(pj_status_t)
pj_media_session_activate (pj_media_session_t *session)
{
    unsigned i;
    pj_status_t status = 0;

    for (i=0; i<session->stream_cnt; ++i) {
	pj_status_t rc;
	rc = pj_media_session_activate_stream (session, i);
	if (status == 0)
	    status = rc;
    }
    return status;
}

/**
 * Activate individual stream in media session.
 */
PJ_DEF(pj_status_t)
pj_media_session_activate_stream (pj_media_session_t *session, unsigned index)
{
    pj_media_stream_desc *sd;
    pj_media_stream_create_param scp;
    pj_status_t status;
    pj_time_val tv;

    if (index < 0 || index >= session->stream_cnt) {
	pj_assert(0);
	return -1;
    }

    sd = session->stream_desc[index];

    if (sd->enc_stream || sd->dec_stream) {
	/* Stream already active. */
	pj_assert(0);
	return 0;
    }

    pj_gettimeofday(&tv);

    /* Initialize parameter to create stream. */
    pj_memset (&scp, 0, sizeof(scp));
    scp.codec_id = &sd->info.fmt[0];
    scp.mediamgr = session->mediamgr;
    scp.dir = sd->info.dir;
    scp.rtp_sock = sd->info.sock_info.rtp_sock;
    scp.rtcp_sock = sd->info.sock_info.rtcp_sock;
    scp.remote_addr = pj_pool_calloc (session->pool, 1, sizeof(pj_sockaddr_in));
    pj_sockaddr_init (scp.remote_addr, &sd->info.rem_addr, sd->info.rem_port);
    scp.ssrc = tv.sec;
    scp.jb_min = 1;
    scp.jb_max = 15;
    scp.jb_maxcnt = 16;

    /* Build the stream! */
    status = pj_media_stream_create (session->pool, &sd->enc_stream, &sd->dec_stream, &scp);

    if (status==0 && sd->enc_stream) {
	status = pj_media_stream_start (sd->enc_stream);
	if (status != 0)
	    goto on_error;
    }
    if (status==0 && sd->dec_stream) {
	status = pj_media_stream_start (sd->dec_stream);
	if (status != 0)
	    goto on_error;
    }
    return status;

on_error:
    if (sd->enc_stream) {
	pj_media_stream_destroy (sd->enc_stream);
	sd->enc_stream = NULL;
    }
    if (sd->dec_stream) {
	pj_media_stream_destroy (sd->dec_stream);
	sd->dec_stream = NULL;
    }
    return status;
}

/**
 * Destroy media session.
 */
PJ_DEF(pj_status_t)
pj_media_session_destroy (pj_media_session_t *session)
{
    unsigned i;

    if (!session)
	return -1;

    for (i=0; i<session->stream_cnt; ++i) {
	pj_media_stream_desc *sd = session->stream_desc[i];

	if (sd->enc_stream) {
	    pj_media_stream_destroy (sd->enc_stream);
	    sd->enc_stream = NULL;
	}
	if (sd->dec_stream) {
	    pj_media_stream_destroy (sd->dec_stream);
	    sd->dec_stream = NULL;
	}
    }
    pj_pool_release (session->pool);
    return 0;
}

