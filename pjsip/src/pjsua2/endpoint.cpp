/* $Id$ */
/* 
 * Copyright (C) 2012 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua2/endpoint.hpp>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE		"endpoint.cpp"
#define MAX_STUN_SERVERS	32
#define TIMER_SIGNATURE		0x600D878A

struct UserTimer
{
    pj_uint32_t		signature;
    OnTimerParam	prm;
    pj_timer_entry	entry;
};


///////////////////////////////////////////////////////////////////////////////

UaConfig::UaConfig()
{
    pjsua_config ua_cfg;

    pjsua_config_default(&ua_cfg);
    fromPj(ua_cfg);
}

void UaConfig::fromPj(const pjsua_config &ua_cfg)
{
    unsigned i;

    this->maxCalls = ua_cfg.max_calls;
    this->threadCnt = ua_cfg.thread_cnt;
    this->userAgent = pj2Str(ua_cfg.user_agent);

    for (i=0; i<ua_cfg.nameserver_count; ++i) {
	this->nameserver.push_back(pj2Str(ua_cfg.nameserver[i]));
    }

    for (i=0; i<ua_cfg.stun_srv_cnt; ++i) {
	this->stunServer.push_back(pj2Str(ua_cfg.stun_srv[i]));
    }

    this->stunIgnoreFailure = ua_cfg.stun_ignore_failure;
    this->natTypeInSdp = ua_cfg.nat_type_in_sdp;
    this->mwiUnsolicitedEnabled = ua_cfg.enable_unsolicited_mwi;
}

pjsua_config UaConfig::toPj() const
{
    unsigned i;
    pjsua_config pua_cfg;

    pjsua_config_default(&pua_cfg);

    pua_cfg.max_calls = this->maxCalls;
    pua_cfg.thread_cnt = this->threadCnt;
    pua_cfg.user_agent = str2Pj(this->userAgent);

    for (i=0; i<this->nameserver.size() && i<PJ_ARRAY_SIZE(pua_cfg.nameserver);
	 ++i)
    {
	pua_cfg.nameserver[i] = str2Pj(this->nameserver[i]);
    }
    pua_cfg.nameserver_count = i;

    for (i=0; i<this->stunServer.size() && i<PJ_ARRAY_SIZE(pua_cfg.stun_srv);
	 ++i)
    {
	pua_cfg.stun_srv[i] = str2Pj(this->stunServer[i]);
    }
    pua_cfg.stun_srv_cnt = i;

    pua_cfg.nat_type_in_sdp = this->natTypeInSdp;
    pua_cfg.enable_unsolicited_mwi = this->mwiUnsolicitedEnabled;

    return pua_cfg;
}

///////////////////////////////////////////////////////////////////////////////

LogConfig::LogConfig()
{
    pjsua_logging_config lc;

    pjsua_logging_config_default(&lc);
    fromPj(lc);
}

void LogConfig::fromPj(const pjsua_logging_config &lc)
{
    this->msgLogging = lc.msg_logging;
    this->level = lc.level;
    this->consoleLevel = lc.console_level;
    this->decor = lc.decor;
    this->filename = pj2Str(lc.log_filename);
    this->fileFlags = lc.log_file_flags;
    this->writer = NULL;
}

pjsua_logging_config LogConfig::toPj() const
{
    pjsua_logging_config lc;

    pjsua_logging_config_default(&lc);

    lc.msg_logging = this->msgLogging;
    lc.level = this->level;
    lc.console_level = this->consoleLevel;
    lc.decor = this->decor;
    lc.log_file_flags = this->fileFlags;
    lc.log_filename = str2Pj(this->filename);

    return lc;
}

///////////////////////////////////////////////////////////////////////////////

MediaConfig::MediaConfig()
{
    pjsua_media_config mc;

    pjsua_media_config_default(&mc);
    fromPj(mc);
}

void MediaConfig::fromPj(const pjsua_media_config &mc)
{
    this->clockRate = mc.clock_rate;
    this->sndClockRate = mc.snd_clock_rate;
    this->channelCount = mc.channel_count;
    this->audioFramePtime = mc.audio_frame_ptime;
    this->maxMediaPorts = mc.max_media_ports;
    this->hasIoqueue = mc.has_ioqueue;
    this->threadCnt = mc.thread_cnt;
    this->quality = mc.quality;
    this->ptime = mc.ptime;
    this->noVad = mc.no_vad;
    this->ilbcMode = mc.ilbc_mode;
    this->txDropPct = mc.tx_drop_pct;
    this->rxDropPct = mc.rx_drop_pct;
    this->ecOptions = mc.ec_options;
    this->ecTailLen = mc.ec_tail_len;
    this->sndRecLatency = mc.snd_rec_latency;
    this->sndPlayLatency = mc.snd_play_latency;
    this->jbInit = mc.jb_init;
    this->jbMinPre = mc.jb_min_pre;
    this->jbMaxPre = mc.jb_max_pre;
    this->jbMax = mc.jb_max;
    this->sndAutoCloseTime = mc.snd_auto_close_time;
    this->vidPreviewEnableNative = mc.vid_preview_enable_native;
}

pjsua_media_config MediaConfig::toPj() const
{
    pjsua_media_config mcfg;

    pjsua_media_config_default(&mcfg);

    mcfg.clock_rate = this->clockRate;
    mcfg.snd_clock_rate = this->sndClockRate;
    mcfg.channel_count = this->channelCount;
    mcfg.audio_frame_ptime = this->audioFramePtime;
    mcfg.max_media_ports = this->maxMediaPorts;
    mcfg.has_ioqueue = this->hasIoqueue;
    mcfg.thread_cnt = this->threadCnt;
    mcfg.quality = this->quality;
    mcfg.ptime = this->ptime;
    mcfg.no_vad = this->noVad;
    mcfg.ilbc_mode = this->ilbcMode;
    mcfg.tx_drop_pct = this->txDropPct;
    mcfg.rx_drop_pct = this->rxDropPct;
    mcfg.ec_options = this->ecOptions;
    mcfg.ec_tail_len = this->ecTailLen;
    mcfg.snd_rec_latency = this->sndRecLatency;
    mcfg.snd_play_latency = this->sndPlayLatency;
    mcfg.jb_init = this->jbInit;
    mcfg.jb_min_pre = this->jbMinPre;
    mcfg.jb_max_pre = this->jbMaxPre;
    mcfg.jb_max = this->jbMax;
    mcfg.snd_auto_close_time = this->sndAutoCloseTime;
    mcfg.vid_preview_enable_native = this->vidPreviewEnableNative;

    return mcfg;
}


///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint instance
 */
Endpoint::Endpoint()
: writer(NULL), epCallback(NULL)
{
}

Endpoint& Endpoint::instance()
{
    static Endpoint lib_;
    return lib_;
}

void Endpoint::testException() throw(Error)
{
    PJSUA2_CHECK_RAISE_ERROR(PJ_EINVALIDOP);
}


///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint static callbacks
 */
void Endpoint::logFunc(int level, const char *data, int len)
{
    Endpoint &ep = Endpoint::instance();

    if (!ep.writer)
	return;

    LogEntry entry;
    entry.level = level;
    entry.msg = string(data, len);
    entry.threadId = (long)pj_thread_this();
    entry.threadName = string(pj_thread_get_name(pj_thread_this()));

    ep.writer->write(entry);
}

void Endpoint::stun_resolve_cb(const pj_stun_resolve_result *res)
{
    Endpoint &ep = Endpoint::instance();

    if (!ep.epCallback || !res)
	return;

    OnNatCheckStunServersCompleteParam prm;

    prm.userData = res->token;
    prm.status = res->status;
    if (res->status == PJ_SUCCESS) {
	char straddr[PJ_INET6_ADDRSTRLEN+10];

	prm.name = string(res->name.ptr, res->name.slen);
	pj_sockaddr_print(&res->addr, straddr, sizeof(straddr), 3);
	prm.addr = straddr;
    }

    ep.epCallback->onNatCheckStunServersComplete(prm);
}

void Endpoint::on_timer(pj_timer_heap_t *timer_heap,
                        pj_timer_entry *entry)
{
    Endpoint &ep = Endpoint::instance();
    UserTimer *ut = (UserTimer*) entry->user_data;

    if (!ep.epCallback || ut->signature != TIMER_SIGNATURE)
	return;

    ep.epCallback->onTimer(ut->prm);
}

void Endpoint::on_nat_detect(const pj_stun_nat_detect_result *res)
{
    Endpoint &ep = Endpoint::instance();

    if (!ep.epCallback || !res)
	return;

    OnNatDetectionCompleteParam prm;

    prm.status = res->status;
    prm.reason = res->status_text;
    prm.natType = res->nat_type;
    prm.natTypeName = res->nat_type_name;

    ep.epCallback->onNatDetectionComplete(prm);
}

void Endpoint::on_transport_state( pjsip_transport *tp,
				   pjsip_transport_state state,
				   const pjsip_transport_state_info *info)
{
    Endpoint &ep = Endpoint::instance();

    if (!ep.epCallback)
	return;

    OnTransportStateParam prm;

    prm.hnd = (TransportHandle)tp;
    prm.state = state;
    prm.lastError = info ? info->status : PJ_SUCCESS;

    ep.epCallback->onTransportState(prm);
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint library operations
 */
void Endpoint::libCreate() throw(Error)
{
    pj_status_t status;

    status = pjsua_create();
    PJSUA2_CHECK_RAISE_ERROR(status);
}

pjsua_state Endpoint::libGetState() const
{
    return pjsua_get_state();
}

void Endpoint::libInit( const EpConfig &prmEpConfig,
                        EpCallback *prmCb) throw(Error)
{
    pjsua_config ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config med_cfg;
    pj_status_t status;

    ua_cfg = prmEpConfig.uaConfig.toPj();
    log_cfg = prmEpConfig.logConfig.toPj();
    med_cfg = prmEpConfig.medConfig.toPj();

    /* Setup log callback */
    if (prmEpConfig.logConfig.writer) {
	this->writer = prmEpConfig.logConfig.writer;
	log_cfg.cb = &Endpoint::logFunc;
    }

    /* Setup UA callbacks */
    pj_bzero(&ua_cfg.cb, sizeof(ua_cfg.cb));
    ua_cfg.cb.on_nat_detect = &Endpoint::on_nat_detect;
    ua_cfg.cb.on_transport_state = &Endpoint::on_transport_state;

    /* Init! */
    status = pjsua_init(&ua_cfg, &log_cfg, &med_cfg);
    PJSUA2_CHECK_RAISE_ERROR(status);
}

void Endpoint::libStart() throw(Error)
{
    pj_status_t status;

    status = pjsua_start();
    PJSUA2_CHECK_RAISE_ERROR(status);
}

void Endpoint::libDestroy(unsigned flags) throw(Error)
{
    pj_status_t status;

    status = pjsua_destroy2(flags);

    this->writer = NULL;
    this->epCallback = NULL;

    if (pj_log_get_log_func() == &Endpoint::logFunc) {
	pj_log_set_log_func(NULL);
    }

    PJSUA2_CHECK_RAISE_ERROR(status);
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint Utilities
 */
string Endpoint::utilStrError(pj_status_t prmErr)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(prmErr, errmsg, sizeof(errmsg));
    return errmsg;
}

static void ept_log_write(int level, const char *sender,
                          const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(sender, level, format, arg );
    va_end(arg);
}

void Endpoint::utilLogWrite(int prmLevel,
			    const string &prmSender,
			    const string &prmMsg)
{
    ept_log_write(prmLevel, prmSender.c_str(), "%s", prmMsg.c_str());
}

pj_status_t Endpoint::utilVerifySipUri(const string &prmUri)
{
    return pjsua_verify_sip_url(prmUri.c_str());
}

pj_status_t Endpoint::utilVerifyUri(const string &prmUri)
{
    return pjsua_verify_url(prmUri.c_str());
}

Token Endpoint::utilTimerSchedule(unsigned prmMsecDelay,
                                  Token prmUserData) throw (Error)
{
    UserTimer *ut;
    pj_time_val delay;
    pj_status_t status;

    ut = new UserTimer;
    ut->signature = TIMER_SIGNATURE;
    ut->prm.msecDelay = prmMsecDelay;
    ut->prm.userData = prmUserData;
    pj_timer_entry_init(&ut->entry, 1, ut, &Endpoint::on_timer);

    delay.sec = 0;
    delay.msec = prmMsecDelay;
    pj_time_val_normalize(&delay);

    status = pjsua_schedule_timer(&ut->entry, &delay);
    if (status != PJ_SUCCESS) {
	delete ut;
	PJSUA2_CHECK_RAISE_ERROR(status);
    }

    return (Token)ut;
}

void Endpoint::utilTimerCancel(Token prmTimerToken)
{
    UserTimer *ut = (UserTimer*)(void*)prmTimerToken;

    if (ut->signature != TIMER_SIGNATURE) {
	PJ_LOG(1,(THIS_FILE,
		  "Invalid timer token in Endpoint::utilTimerCancel()"));
	return;
    }

    ut->entry.id = 0;
    ut->signature = 0xFFFFFFFE;
    pjsua_cancel_timer(&ut->entry);

    delete ut;
}

IntVector Endpoint::utilSslGetAvailableCiphers() throw (Error)
{
#if PJ_HAS_SSL_SOCK
    pj_ssl_cipher ciphers[64];
    unsigned count = PJ_ARRAY_SIZE(ciphers);
    pj_status_t status;

    status = pj_ssl_cipher_get_availables(ciphers, &count);
    PJSUA2_CHECK_RAISE_ERROR(status);

    return IntVector(ciphers, ciphers + count);
#else
    return IntVector();
#endif
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint NAT operations
 */
void Endpoint::natDetectType(void) throw(Error)
{
    pj_status_t status;

    status = pjsua_detect_nat_type();
    PJSUA2_CHECK_RAISE_ERROR(status);
}

pj_stun_nat_type Endpoint::natGetType() throw(Error)
{
    pj_stun_nat_type type;
    pj_status_t status;

    status = pjsua_get_nat_type(&type);
    PJSUA2_CHECK_RAISE_ERROR(status);

    return type;
}

void Endpoint::natCheckStunServers(const StringVector &servers,
				   bool wait,
				   Token token) throw(Error)
{
    pj_str_t srv[MAX_STUN_SERVERS];
    unsigned i, count = 0;
    pj_status_t status;

    for (i=0; i<servers.size() && i<MAX_STUN_SERVERS; ++i) {
	srv[count].ptr = (char*)servers[i].c_str();
	srv[count].slen = servers[i].size();
	++count;
    }

    status = pjsua_resolve_stun_servers(count, srv, wait, token,
    					&Endpoint::stun_resolve_cb);
    PJSUA2_CHECK_RAISE_ERROR(status);
}

void Endpoint::natCancelCheckStunServers(Token token,
                                         bool notify_cb) throw(Error)
{
    pj_status_t status;

    status = pjsua_cancel_stun_resolution(token, notify_cb);
    PJSUA2_CHECK_RAISE_ERROR(status);
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Transport API
 */
TransportId Endpoint::transportCreate(pjsip_transport_type_e type,
                                      const TransportConfig &cfg) throw(Error)
{
    pjsua_transport_config tcfg;
    pjsua_transport_id tid;
    pj_status_t status;

    tcfg = cfg.toPj();
    status = pjsua_transport_create(type, &tcfg, &tid);
    PJSUA2_CHECK_RAISE_ERROR(status);

    return tid;
}

IntVector Endpoint::transportEnum() throw(Error)
{
    pjsua_transport_id tids[32];
    unsigned count = PJ_ARRAY_SIZE(tids);
    pj_status_t status;

    status = pjsua_enum_transports(tids, &count);
    PJSUA2_CHECK_RAISE_ERROR(status);

    return IntVector(tids, tids+count);
}

TransportInfo Endpoint::transportGetInfo(TransportId id) throw(Error)
{
    pjsua_transport_info tinfo;
    pj_status_t status;

    status = pjsua_transport_get_info(id, &tinfo);
    PJSUA2_CHECK_RAISE_ERROR(status);

    return TransportInfo(tinfo);
}

void Endpoint::transportSetEnable(TransportId id, bool enabled) throw(Error)
{
    pj_status_t status;

    status = pjsua_transport_set_enable(id, enabled);
    PJSUA2_CHECK_RAISE_ERROR(status);
}

void Endpoint::transportClose(TransportId id) throw(Error)
{
    pj_status_t status;

    status = pjsua_transport_close(id, PJ_FALSE);
    PJSUA2_CHECK_RAISE_ERROR(status);
}

///////////////////////////////////////////////////////////////////////////////

