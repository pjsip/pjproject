/* $Id$ */
/* 
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua2/account.hpp>
#include <pjsua2/call.hpp>
#include <pjsua2/presence.hpp>
#include <algorithm>
#include "util.hpp"

using namespace pj;
using namespace std;

#include <pjsua-lib/pjsua_internal.h>   /* For retrieving pjsua threads */

#define THIS_FILE		"endpoint.cpp"
#define MAX_STUN_SERVERS	32
#define TIMER_SIGNATURE		0x600D878A
#define MAX_CODEC_NUM 		64

struct UserTimer
{
    pj_uint32_t		signature;
    OnTimerParam	prm;
    pj_timer_entry	entry;
};

Endpoint *Endpoint::instance_;

///////////////////////////////////////////////////////////////////////////////

TlsInfo::TlsInfo() : cipher(PJ_TLS_UNKNOWN_CIPHER),
		     empty(true)
{
}

bool TlsInfo::isEmpty() const
{
    return empty;
}

void TlsInfo::fromPj(const pjsip_tls_state_info &info)
{
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0
    pj_ssl_sock_info *ssock_info = info.ssl_sock_info;
    char straddr[PJ_INET6_ADDRSTRLEN+10];
    const char *verif_msgs[32];
    unsigned verif_msg_cnt;
    
    empty	= false;
    established = PJ2BOOL(ssock_info->established);
    protocol 	= ssock_info->proto;
    cipher 	= ssock_info->cipher;
    cipherName	= pj_ssl_cipher_name(ssock_info->cipher);
    pj_sockaddr_print(&ssock_info->local_addr, straddr, sizeof(straddr), 3);
    localAddr 	= straddr;
    pj_sockaddr_print(&ssock_info->remote_addr, straddr, sizeof(straddr),3);
    remoteAddr 	= straddr;
    verifyStatus = ssock_info->verify_status;
    if (ssock_info->local_cert_info)
        localCertInfo.fromPj(*ssock_info->local_cert_info);
    if (ssock_info->remote_cert_info)
        remoteCertInfo.fromPj(*ssock_info->remote_cert_info);
    
    /* Dump server TLS certificate verification result */
    verif_msg_cnt = PJ_ARRAY_SIZE(verif_msgs);
    pj_ssl_cert_get_verify_status_strings(ssock_info->verify_status,
    				      	  verif_msgs, &verif_msg_cnt);
    for (unsigned i = 0; i < verif_msg_cnt; ++i) {
        verifyMsgs.push_back(verif_msgs[i]);
    }
#else
    PJ_UNUSED_ARG(info);
#endif
}

SslCertInfo::SslCertInfo()
	: empty(true)
{
}

bool SslCertInfo::isEmpty() const
{
    return empty;
}

void SslCertInfo::fromPj(const pj_ssl_cert_info &info)
{
    empty 	= false;
    version 	= info.version;
    pj_memcpy(serialNo, info.serial_no, sizeof(info.serial_no));
    subjectCn 	= pj2Str(info.subject.cn);
    subjectInfo = pj2Str(info.subject.info);
    issuerCn 	= pj2Str(info.issuer.cn);
    issuerInfo 	= pj2Str(info.issuer.info);
    validityStart.fromPj(info.validity.start);
    validityEnd.fromPj(info.validity.end);
    validityGmt = PJ2BOOL(info.validity.gmt);
    raw 	= pj2Str(info.raw);

    for (unsigned i = 0; i < info.subj_alt_name.cnt; i++) {
    	SslCertName cname;
    	cname.type = info.subj_alt_name.entry[i].type;
    	cname.name = pj2Str(info.subj_alt_name.entry[i].name);
    	subjectAltName.push_back(cname);
    }
}

///////////////////////////////////////////////////////////////////////////////
IpChangeParam::IpChangeParam()
{
    pjsua_ip_change_param param;    
    pjsua_ip_change_param_default(&param);
    fromPj(param);
}


pjsua_ip_change_param IpChangeParam::toPj() const
{
    pjsua_ip_change_param param;
    pjsua_ip_change_param_default(&param);

    param.restart_listener = restartListener;
    param.restart_lis_delay = restartLisDelay;

    return param;
}


void IpChangeParam::fromPj(const pjsua_ip_change_param &param)
{
    restartListener = PJ2BOOL(param.restart_listener);
    restartLisDelay = param.restart_lis_delay;
}

///////////////////////////////////////////////////////////////////////////////
UaConfig::UaConfig()
: mainThreadOnly(false)
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
    for (i=0; i<ua_cfg.outbound_proxy_cnt; ++i) {
	this->outboundProxies.push_back(pj2Str(ua_cfg.outbound_proxy[i]));
    }

    this->stunTryIpv6 = PJ2BOOL(ua_cfg.stun_try_ipv6);
    this->stunIgnoreFailure = PJ2BOOL(ua_cfg.stun_ignore_failure);
    this->natTypeInSdp = ua_cfg.nat_type_in_sdp;
    this->mwiUnsolicitedEnabled = PJ2BOOL(ua_cfg.enable_unsolicited_mwi);
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

    for (i=0; i<this->outboundProxies.size() &&
    	      i<PJ_ARRAY_SIZE(pua_cfg.outbound_proxy); ++i)
    {
	pua_cfg.outbound_proxy[i] = str2Pj(this->outboundProxies[i]);
    }
    pua_cfg.outbound_proxy_cnt= i;

    pua_cfg.nat_type_in_sdp = this->natTypeInSdp;
    pua_cfg.enable_unsolicited_mwi = this->mwiUnsolicitedEnabled;
    pua_cfg.stun_try_ipv6 = this->stunTryIpv6;
    pua_cfg.stun_ignore_failure = this->stunIgnoreFailure;

    return pua_cfg;
}

void UaConfig::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.readContainer("UaConfig");

    NODE_READ_UNSIGNED( this_node, maxCalls);
    NODE_READ_UNSIGNED( this_node, threadCnt);
    NODE_READ_BOOL    ( this_node, mainThreadOnly);
    NODE_READ_STRINGV ( this_node, nameserver);
    NODE_READ_STRING  ( this_node, userAgent);
    NODE_READ_STRINGV ( this_node, stunServer);
    NODE_READ_BOOL    ( this_node, stunTryIpv6);
    NODE_READ_BOOL    ( this_node, stunIgnoreFailure);
    NODE_READ_INT     ( this_node, natTypeInSdp);
    NODE_READ_BOOL    ( this_node, mwiUnsolicitedEnabled);
}

void UaConfig::writeObject(ContainerNode &node) const PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.writeNewContainer("UaConfig");

    NODE_WRITE_UNSIGNED( this_node, maxCalls);
    NODE_WRITE_UNSIGNED( this_node, threadCnt);
    NODE_WRITE_BOOL    ( this_node, mainThreadOnly);
    NODE_WRITE_STRINGV ( this_node, nameserver);
    NODE_WRITE_STRING  ( this_node, userAgent);
    NODE_WRITE_STRINGV ( this_node, stunServer);
    NODE_WRITE_BOOL    ( this_node, stunTryIpv6);
    NODE_WRITE_BOOL    ( this_node, stunIgnoreFailure);
    NODE_WRITE_INT     ( this_node, natTypeInSdp);
    NODE_WRITE_BOOL    ( this_node, mwiUnsolicitedEnabled);
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

void LogConfig::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.readContainer("LogConfig");

    NODE_READ_UNSIGNED( this_node, msgLogging);
    NODE_READ_UNSIGNED( this_node, level);
    NODE_READ_UNSIGNED( this_node, consoleLevel);
    NODE_READ_UNSIGNED( this_node, decor);
    NODE_READ_STRING  ( this_node, filename);
    NODE_READ_UNSIGNED( this_node, fileFlags);
}

void LogConfig::writeObject(ContainerNode &node) const PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.writeNewContainer("LogConfig");

    NODE_WRITE_UNSIGNED( this_node, msgLogging);
    NODE_WRITE_UNSIGNED( this_node, level);
    NODE_WRITE_UNSIGNED( this_node, consoleLevel);
    NODE_WRITE_UNSIGNED( this_node, decor);
    NODE_WRITE_STRING  ( this_node, filename);
    NODE_WRITE_UNSIGNED( this_node, fileFlags);
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
    this->hasIoqueue = PJ2BOOL(mc.has_ioqueue);
    this->threadCnt = mc.thread_cnt;
    this->quality = mc.quality;
    this->ptime = mc.ptime;
    this->noVad = PJ2BOOL(mc.no_vad);
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
    this->jbDiscardAlgo = mc.jb_discard_algo;
    this->sndAutoCloseTime = mc.snd_auto_close_time;
    this->vidPreviewEnableNative = PJ2BOOL(mc.vid_preview_enable_native);
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
    mcfg.jb_discard_algo = this->jbDiscardAlgo;
    mcfg.snd_auto_close_time = this->sndAutoCloseTime;
    mcfg.vid_preview_enable_native = this->vidPreviewEnableNative;

    return mcfg;
}

void MediaConfig::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.readContainer("MediaConfig");

    NODE_READ_UNSIGNED( this_node, clockRate);
    NODE_READ_UNSIGNED( this_node, sndClockRate);
    NODE_READ_UNSIGNED( this_node, channelCount);
    NODE_READ_UNSIGNED( this_node, audioFramePtime);
    NODE_READ_UNSIGNED( this_node, maxMediaPorts);
    NODE_READ_BOOL    ( this_node, hasIoqueue);
    NODE_READ_UNSIGNED( this_node, threadCnt);
    NODE_READ_UNSIGNED( this_node, quality);
    NODE_READ_UNSIGNED( this_node, ptime);
    NODE_READ_BOOL    ( this_node, noVad);
    NODE_READ_UNSIGNED( this_node, ilbcMode);
    NODE_READ_UNSIGNED( this_node, txDropPct);
    NODE_READ_UNSIGNED( this_node, rxDropPct);
    NODE_READ_UNSIGNED( this_node, ecOptions);
    NODE_READ_UNSIGNED( this_node, ecTailLen);
    NODE_READ_UNSIGNED( this_node, sndRecLatency);
    NODE_READ_UNSIGNED( this_node, sndPlayLatency);
    NODE_READ_INT     ( this_node, jbInit);
    NODE_READ_INT     ( this_node, jbMinPre);
    NODE_READ_INT     ( this_node, jbMaxPre);
    NODE_READ_INT     ( this_node, jbMax);
    NODE_READ_NUM_T   ( this_node, pjmedia_jb_discard_algo, jbDiscardAlgo);
    NODE_READ_INT     ( this_node, sndAutoCloseTime);
    NODE_READ_BOOL    ( this_node, vidPreviewEnableNative);
}

void MediaConfig::writeObject(ContainerNode &node) const PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.writeNewContainer("MediaConfig");

    NODE_WRITE_UNSIGNED( this_node, clockRate);
    NODE_WRITE_UNSIGNED( this_node, sndClockRate);
    NODE_WRITE_UNSIGNED( this_node, channelCount);
    NODE_WRITE_UNSIGNED( this_node, audioFramePtime);
    NODE_WRITE_UNSIGNED( this_node, maxMediaPorts);
    NODE_WRITE_BOOL    ( this_node, hasIoqueue);
    NODE_WRITE_UNSIGNED( this_node, threadCnt);
    NODE_WRITE_UNSIGNED( this_node, quality);
    NODE_WRITE_UNSIGNED( this_node, ptime);
    NODE_WRITE_BOOL    ( this_node, noVad);
    NODE_WRITE_UNSIGNED( this_node, ilbcMode);
    NODE_WRITE_UNSIGNED( this_node, txDropPct);
    NODE_WRITE_UNSIGNED( this_node, rxDropPct);
    NODE_WRITE_UNSIGNED( this_node, ecOptions);
    NODE_WRITE_UNSIGNED( this_node, ecTailLen);
    NODE_WRITE_UNSIGNED( this_node, sndRecLatency);
    NODE_WRITE_UNSIGNED( this_node, sndPlayLatency);
    NODE_WRITE_INT     ( this_node, jbInit);
    NODE_WRITE_INT     ( this_node, jbMinPre);
    NODE_WRITE_INT     ( this_node, jbMaxPre);
    NODE_WRITE_INT     ( this_node, jbMax);
    NODE_WRITE_NUM_T   ( this_node, pjmedia_jb_discard_algo, jbDiscardAlgo);
    NODE_WRITE_INT     ( this_node, sndAutoCloseTime);
    NODE_WRITE_BOOL    ( this_node, vidPreviewEnableNative);
}

///////////////////////////////////////////////////////////////////////////////

void EpConfig::readObject(const ContainerNode &node) PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.readContainer("EpConfig");
    NODE_READ_OBJ( this_node, uaConfig);
    NODE_READ_OBJ( this_node, logConfig);
    NODE_READ_OBJ( this_node, medConfig);
}

void EpConfig::writeObject(ContainerNode &node) const PJSUA2_THROW(Error)
{
    ContainerNode this_node = node.writeNewContainer("EpConfig");
    NODE_WRITE_OBJ( this_node, uaConfig);
    NODE_WRITE_OBJ( this_node, logConfig);
    NODE_WRITE_OBJ( this_node, medConfig);
}

///////////////////////////////////////////////////////////////////////////////
/* Class to post log to main thread */
struct PendingLog : public PendingJob
{
    LogEntry entry;
    virtual void execute(bool is_pending)
    {
	PJ_UNUSED_ARG(is_pending);
	Endpoint::instance().utilLogWrite(entry);
    }
};

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint instance
 */
Endpoint::Endpoint():
#if !DEPRECATED_FOR_TICKET_2232
mediaListMutex(NULL),
#endif
writer(NULL), threadDescMutex(NULL), mainThreadOnly(false),
mainThread(NULL), pendingJobSize(0)
{
    if (instance_) {
	PJSUA2_RAISE_ERROR(PJ_EEXISTS);
    }

    instance_ = this;
}

Endpoint& Endpoint::instance() PJSUA2_THROW(Error)
{
    if (!instance_) {
	PJSUA2_RAISE_ERROR(PJ_ENOTFOUND);
    }
    return *instance_;
}

Endpoint::~Endpoint()
{
    while (!pendingJobs.empty()) {
	delete pendingJobs.front();
	pendingJobs.pop_front();
    }

#if !DEPRECATED_FOR_TICKET_2232
    clearCodecInfoList(codecInfoList);
    clearCodecInfoList(videoCodecInfoList);
#endif

    try {
	libDestroy();
    } catch (Error &err) {
	// Ignore
	PJ_UNUSED_ARG(err);
    }

    instance_ = NULL;
}

void Endpoint::utilAddPendingJob(PendingJob *job)
{
    enum {
	MAX_PENDING_JOBS = 1024
    };

    /* See if we can execute immediately */
    if (!mainThreadOnly || pj_thread_this()==mainThread) {
	job->execute(false);
	delete job;
	return;
    }

    if (pendingJobSize > MAX_PENDING_JOBS) {
	enum { NUMBER_TO_DISCARD = 5 };

	pj_enter_critical_section();
	for (unsigned i=0; i<NUMBER_TO_DISCARD; ++i) {
	    delete pendingJobs.back();
	    pendingJobs.pop_back();
	}

	pendingJobSize -= NUMBER_TO_DISCARD;
	pj_leave_critical_section();

	utilLogWrite(1, THIS_FILE,
	             "*** ERROR: Job queue full!! Jobs discarded!!! ***");
    }

    pj_enter_critical_section();
    pendingJobs.push_back(job);
    pendingJobSize++;
    pj_leave_critical_section();
}

/* Handle log callback */
void Endpoint::utilLogWrite(LogEntry &entry)
{
    if (!writer) return;

    if (mainThreadOnly && pj_thread_this() != mainThread) {
	PendingLog *job = new PendingLog;
	job->entry = entry;
	utilAddPendingJob(job);
    } else {
	writer->write(entry);
    }
}

/* Run pending jobs only in main thread */
void Endpoint::performPendingJobs()
{
    if (pj_thread_this() != mainThread)
	return;

    if (pendingJobSize == 0)
	return;

    for (;;) {
	PendingJob *job = NULL;

	pj_enter_critical_section();
	if (pendingJobSize != 0) {
	    job = pendingJobs.front();
	    pendingJobs.pop_front();
	    pendingJobSize--;
	}
	pj_leave_critical_section();

	if (job) {
	    job->execute(true);
	    delete job;
	} else
	    break;
    }
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
    entry.threadId = (long)(size_t)pj_thread_this();
    entry.threadName = string(pj_thread_get_name(pj_thread_this()));

    ep.utilLogWrite(entry);
}

void Endpoint::stun_resolve_cb(const pj_stun_resolve_result *res)
{
    Endpoint &ep = Endpoint::instance();

    if (!res)
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

    ep.onNatCheckStunServersComplete(prm);
}

void Endpoint::on_timer(pj_timer_heap_t *timer_heap,
                        pj_timer_entry *entry)
{
    PJ_UNUSED_ARG(timer_heap);

    Endpoint &ep = Endpoint::instance();
    UserTimer *ut = (UserTimer*) entry->user_data;

    if (ut->signature != TIMER_SIGNATURE)
	return;

    ep.onTimer(ut->prm);
}

void Endpoint::on_nat_detect(const pj_stun_nat_detect_result *res)
{
    Endpoint &ep = Endpoint::instance();

    if (!res)
	return;

    OnNatDetectionCompleteParam prm;

    prm.status = res->status;
    prm.reason = res->status_text;
    prm.natType = res->nat_type;
    prm.natTypeName = res->nat_type_name;

    ep.onNatDetectionComplete(prm);
}

void Endpoint::on_transport_state( pjsip_transport *tp,
				   pjsip_transport_state state,
				   const pjsip_transport_state_info *info)
{
    Endpoint &ep = Endpoint::instance();

    OnTransportStateParam prm;

    prm.hnd = (TransportHandle)tp;
    prm.type = tp->type_name;
    prm.state = state;
    prm.lastError = info ? info->status : PJ_SUCCESS;

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    if (!pj_ansi_stricmp(tp->type_name, "tls") && info->ext_info &&
	(state == PJSIP_TP_STATE_CONNECTED || 
	 ((pjsip_tls_state_info*)info->ext_info)->
			         ssl_sock_info->verify_status != PJ_SUCCESS))
    {
    	prm.tlsInfo.fromPj(*((pjsip_tls_state_info*)info->ext_info));
    }
#endif

    ep.onTransportState(prm);
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Account static callbacks
 */

Account *Endpoint::lookupAcc(int acc_id, const char *op)
{
    Account *acc = Account::lookup(acc_id);
    if (!acc) {
	PJ_LOG(1,(THIS_FILE,
		  "Error: cannot find Account instance for account id %d in "
		  "%s", acc_id, op));
    }

    return acc;
}

Call *Endpoint::lookupCall(int call_id, const char *op)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	PJ_LOG(1,(THIS_FILE,
		  "Error: cannot find Call instance for call id %d in "
		  "%s", call_id, op));
    }

    return call;
}

void Endpoint::on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                                pjsip_rx_data *rdata)
{
    Account *acc = lookupAcc(acc_id, "on_incoming_call()");
    if (!acc) {
	pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL);
	return;
    }

    pjsua_call *call = &pjsua_var.calls[call_id];
    if (!call->incoming_data) {
	/* This happens when the incoming call callback has been called from 
	 * inside the on_create_media_transport() callback. So we simply 
	 * return here to avoid calling	the callback twice. 
	 */
	return;
    }

    /* call callback */
    OnIncomingCallParam prm;
    prm.callId = call_id;
    prm.rdata.fromPj(*rdata);

    acc->onIncomingCall(prm);

    /* Free cloned rdata. */
    pjsip_rx_data_free_cloned(call->incoming_data);
    call->incoming_data = NULL;

    /* disconnect if callback doesn't handle the call */
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);
    if (!pjsua_call_get_user_data(call_id) &&
	ci.state != PJSIP_INV_STATE_DISCONNECTED)
    {
	pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL);
    }
}

void Endpoint::on_reg_started(pjsua_acc_id acc_id, pj_bool_t renew)
{
    Account *acc = lookupAcc(acc_id, "on_reg_started()");
    if (!acc) {
	return;
    }

    OnRegStartedParam prm;
    prm.renew = PJ2BOOL(renew);
    acc->onRegStarted(prm);
}

void Endpoint::on_reg_state2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
    Account *acc = lookupAcc(acc_id, "on_reg_state2()");
    if (!acc) {
	return;
    }

    OnRegStateParam prm;
    prm.status		= info->cbparam->status;
    prm.code 		= (pjsip_status_code) info->cbparam->code;
    prm.reason		= pj2Str(info->cbparam->reason);
    if (info->cbparam->rdata)
	prm.rdata.fromPj(*info->cbparam->rdata);
    prm.expiration	= info->cbparam->expiration;

    acc->onRegState(prm);
}

void Endpoint::on_incoming_subscribe(pjsua_acc_id acc_id,
                                     pjsua_srv_pres *srv_pres,
                                     pjsua_buddy_id buddy_id,
                                     const pj_str_t *from,
                                     pjsip_rx_data *rdata,
                                     pjsip_status_code *code,
                                     pj_str_t *reason,
                                     pjsua_msg_data *msg_data)
{
    PJ_UNUSED_ARG(buddy_id);
    PJ_UNUSED_ARG(srv_pres);

    Account *acc = lookupAcc(acc_id, "on_incoming_subscribe()");
    if (!acc) {
	/* default behavior should apply */
	return;
    }

    OnIncomingSubscribeParam prm;
    prm.srvPres		= srv_pres;
    prm.fromUri 	= pj2Str(*from);
    prm.rdata.fromPj(*rdata);
    prm.code		= *code;
    prm.reason		= pj2Str(*reason);
    prm.txOption.fromPj(*msg_data);

    acc->onIncomingSubscribe(prm);

    *code = prm.code;
    acc->tmpReason = prm.reason;
    *reason = str2Pj(acc->tmpReason);
    prm.txOption.toPj(*msg_data);
}

void Endpoint::on_pager2(pjsua_call_id call_id,
                         const pj_str_t *from,
                         const pj_str_t *to,
                         const pj_str_t *contact,
                         const pj_str_t *mime_type,
                         const pj_str_t *body,
                         pjsip_rx_data *rdata,
                         pjsua_acc_id acc_id)
{
    OnInstantMessageParam prm;
    prm.fromUri		= pj2Str(*from);
    prm.toUri		= pj2Str(*to);
    prm.contactUri	= pj2Str(*contact);
    prm.contentType	= pj2Str(*mime_type);
    prm.msgBody		= pj2Str(*body);
    prm.rdata.fromPj(*rdata);

    if (call_id != PJSUA_INVALID_ID) {
	Call *call = lookupCall(call_id, "on_pager2()");
	if (!call) {
	    /* Ignored */
	    return;
	}

	call->onInstantMessage(prm);
    } else {
	Account *acc = lookupAcc(acc_id, "on_pager2()");
	if (!acc) {
	    /* Ignored */
	    return;
	}

	acc->onInstantMessage(prm);
    }
}

void Endpoint::on_pager_status2( pjsua_call_id call_id,
				 const pj_str_t *to,
				 const pj_str_t *body,
				 void *user_data,
				 pjsip_status_code status,
				 const pj_str_t *reason,
				 pjsip_tx_data *tdata,
				 pjsip_rx_data *rdata,
				 pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(tdata);

    OnInstantMessageStatusParam prm;
    prm.userData	= user_data;
    prm.toUri		= pj2Str(*to);
    prm.msgBody		= pj2Str(*body);
    prm.code		= status;
    prm.reason		= pj2Str(*reason);
    if (rdata)
	prm.rdata.fromPj(*rdata);

    if (call_id != PJSUA_INVALID_ID) {
	Call *call = lookupCall(call_id, "on_pager_status2()");
	if (!call) {
	    /* Ignored */
	    return;
	}

	call->onInstantMessageStatus(prm);
    } else {
	Account *acc = lookupAcc(acc_id, "on_pager_status2()");
	if (!acc) {
	    /* Ignored */
	    return;
	}

	acc->onInstantMessageStatus(prm);
    }
}

void Endpoint::on_typing2( pjsua_call_id call_id,
			   const pj_str_t *from,
			   const pj_str_t *to,
			   const pj_str_t *contact,
			   pj_bool_t is_typing,
			   pjsip_rx_data *rdata,
			   pjsua_acc_id acc_id)
{
    OnTypingIndicationParam prm;
    prm.fromUri		= pj2Str(*from);
    prm.toUri		= pj2Str(*to);
    prm.contactUri	= pj2Str(*contact);
    prm.isTyping	= is_typing != 0;
    prm.rdata.fromPj(*rdata);

    if (call_id != PJSUA_INVALID_ID) {
	Call *call = lookupCall(call_id, "on_typing2()");
	if (!call) {
	    /* Ignored */
	    return;
	}

	call->onTypingIndication(prm);
    } else {
	Account *acc = lookupAcc(acc_id, "on_typing2()");
	if (!acc) {
	    /* Ignored */
	    return;
	}

	acc->onTypingIndication(prm);
    }
}

void Endpoint::on_mwi_info(pjsua_acc_id acc_id,
                           pjsua_mwi_info *mwi_info)
{
    OnMwiInfoParam prm;

    if (mwi_info->evsub) {
	prm.state	= pjsip_evsub_get_state(mwi_info->evsub);
    } else {
	/* Unsolicited MWI */
	prm.state	= PJSIP_EVSUB_STATE_NULL;
    }
    prm.rdata.fromPj(*mwi_info->rdata);

    Account *acc = lookupAcc(acc_id, "on_mwi_info()");
    if (!acc) {
	/* Ignored */
	return;
    }

    acc->onMwiInfo(prm);
}

void Endpoint::on_acc_find_for_incoming(const pjsip_rx_data *rdata,
				        pjsua_acc_id* acc_id)
{
    OnSelectAccountParam prm;

    pj_assert(rdata && acc_id);
    prm.rdata.fromPj(*((pjsip_rx_data *)rdata));
    prm.accountIndex = *acc_id;
    
    instance_->onSelectAccount(prm);
    
    *acc_id = prm.accountIndex;
}

void Endpoint::on_buddy_state(pjsua_buddy_id buddy_id)
{
    Buddy b(buddy_id);
    Buddy *buddy = b.getOriginalInstance();
    if (!buddy || !buddy->isValid()) {
	/* Ignored */
	return;
    }

    buddy->onBuddyState();
}

void Endpoint::on_buddy_evsub_state(pjsua_buddy_id buddy_id,
				    pjsip_evsub *sub,
				    pjsip_event *event)
{
    PJ_UNUSED_ARG(sub);

    Buddy b(buddy_id);
    Buddy *buddy = b.getOriginalInstance();
    if (!buddy || !buddy->isValid()) {
	/* Ignored */
	return;
    }

    OnBuddyEvSubStateParam prm;
    prm.e.fromPj(*event);

    buddy->onBuddyEvSubState(prm);
}

// Call callbacks
void Endpoint::on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallStateParam prm;
    prm.e.fromPj(*e);
    
    call->processStateChange(prm);
    /* If the state is DISCONNECTED, call may have already been deleted
     * by the application in the callback, so do not access it anymore here.
     */
}

void Endpoint::on_call_tsx_state(pjsua_call_id call_id,
                                 pjsip_transaction *tsx,
                                 pjsip_event *e)
{
    PJ_UNUSED_ARG(tsx);

    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallTsxStateParam prm;
    prm.e.fromPj(*e);
    
    call->onCallTsxState(prm);
}

void Endpoint::on_call_media_state(pjsua_call_id call_id)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }

    OnCallMediaStateParam prm;
    call->processMediaUpdate(prm);
}

void Endpoint::on_call_sdp_created(pjsua_call_id call_id,
                                   pjmedia_sdp_session *sdp,
                                   pj_pool_t *pool,
                                   const pjmedia_sdp_session *rem_sdp)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallSdpCreatedParam prm;
    string orig_sdp;
    
    prm.sdp.fromPj(*sdp);
    orig_sdp = prm.sdp.wholeSdp;
    if (rem_sdp)
        prm.remSdp.fromPj(*rem_sdp);
    
    call->sdp_pool = pool;
    call->onCallSdpCreated(prm);
    
    /* Check if application modifies the SDP */
    if (orig_sdp != prm.sdp.wholeSdp) {
        pjmedia_sdp_session *new_sdp;
        pj_str_t dup_new_sdp;
        pj_str_t new_sdp_str = {(char*)prm.sdp.wholeSdp.c_str(),
        			(pj_ssize_t)prm.sdp.wholeSdp.size()};
	pj_status_t status;

        pj_strdup(pool, &dup_new_sdp, &new_sdp_str);        
        status = pjmedia_sdp_parse(pool, dup_new_sdp.ptr,
				   dup_new_sdp.slen, &new_sdp);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, status,
			 "Failed to parse the modified SDP"));
	} else {
	    pj_memcpy(sdp, new_sdp, sizeof(*sdp));
	}
    }
}

void Endpoint::on_stream_created2(pjsua_call_id call_id,
				  pjsua_on_stream_created_param *param)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnStreamCreatedParam prm;
    prm.stream = param->stream;
    prm.streamIdx = param->stream_idx;
    prm.destroyPort = (param->destroy_port != PJ_FALSE);
    prm.pPort = (MediaPort)param->port;
    
    call->onStreamCreated(prm);
    
    param->destroy_port = prm.destroyPort;
    param->port = (pjmedia_port *)prm.pPort;
}

void Endpoint::on_stream_destroyed(pjsua_call_id call_id,
                                   pjmedia_stream *strm,
                                   unsigned stream_idx)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
    	/* This can happen for call disconnection case. The callback
    	 * should have been called from on_call_state() instead.
    	 */
	return;
    }
    
    OnStreamDestroyedParam prm;
    prm.stream = strm;
    prm.streamIdx = stream_idx;
    
    call->onStreamDestroyed(prm);
}

struct PendingOnDtmfDigitCallback : public PendingJob
{
    int call_id;
    OnDtmfDigitParam prm;

    virtual void execute(bool is_pending)
    {
	PJ_UNUSED_ARG(is_pending);

	Call *call = Call::lookup(call_id);
	if (!call)
	    return;

	call->onDtmfDigit(prm);
    }
};

void Endpoint::on_dtmf_digit(pjsua_call_id call_id, int digit)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    PendingOnDtmfDigitCallback *job = new PendingOnDtmfDigitCallback;
    job->call_id = call_id;
    char buf[10];
    pj_ansi_sprintf(buf, "%c", digit);
    job->prm.digit = string(buf);
    
    Endpoint::instance().utilAddPendingJob(job);
}

void Endpoint::on_dtmf_digit2(pjsua_call_id call_id, 
			      const pjsua_dtmf_info *info)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    PendingOnDtmfDigitCallback *job = new PendingOnDtmfDigitCallback;
    job->call_id = call_id;
    char buf[10];
    pj_ansi_sprintf(buf, "%c", info->digit);
    job->prm.digit = string(buf);
    job->prm.method = info->method;
    job->prm.duration = info->duration;
    
    Endpoint::instance().utilAddPendingJob(job);
}

struct PendingOnDtmfEventCallback : public PendingJob
{
    int call_id;
    OnDtmfEventParam prm;

    virtual void execute(bool is_pending)
    {
        PJ_UNUSED_ARG(is_pending);

        Call *call = Call::lookup(call_id);
        if (!call)
            return;

        call->onDtmfEvent(prm);

        /* If this event indicates a new DTMF digit, invoke onDtmfDigit
         * as well.
         * Note that the duration is pretty much useless in this context as it
         * will most likely equal the ptime of one frame received via RTP in
         * milliseconds. Since the application can't receive updates to the
         * duration via this interface and the total duration of the event is
         * not known yet, just indicate an unknown duration.
         */
        if (!(prm.flags & PJMEDIA_STREAM_DTMF_IS_UPDATE)) {
            OnDtmfDigitParam prmBasic;
            prmBasic.method = prm.method;
            prmBasic.digit = prm.digit;
            prmBasic.duration = PJSUA_UNKNOWN_DTMF_DURATION;
            call->onDtmfDigit(prmBasic);
        }
    }
};

void Endpoint::on_dtmf_event(pjsua_call_id call_id,
                             const pjsua_dtmf_event* event)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
        return;
    }

    PendingOnDtmfEventCallback *job = new PendingOnDtmfEventCallback;
    job->call_id = call_id;
    char buf[10];
    pj_ansi_sprintf(buf, "%c", event->digit);
    job->prm.method = event->method;
    job->prm.timestamp = event->timestamp;
    job->prm.digit = string(buf);
    job->prm.duration = event->duration;
    job->prm.flags = event->flags;

    Endpoint::instance().utilAddPendingJob(job);
}

void Endpoint::on_call_transfer_request2(pjsua_call_id call_id,
                                         const pj_str_t *dst,
                                         pjsip_status_code *code,
                                         pjsua_call_setting *opt)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallTransferRequestParam prm;
    prm.dstUri = pj2Str(*dst);
    prm.statusCode = *code;
    prm.opt.fromPj(*opt);
    prm.newCall = NULL;
    
    call->onCallTransferRequest(prm);
    
    *code = prm.statusCode;
    *opt = prm.opt.toPj();
    if (*code/100 <= 2) {
	if (prm.newCall) {
	    /* We don't manage (e.g: create, delete) the call child,
	     * so let's just override any existing child.
	     */
	    call->child = prm.newCall;
	    call->child->id = PJSUA_INVALID_ID;
	} else {
	    PJ_LOG(4,(THIS_FILE,
		      "Warning: application reuses Call instance in "
		      "call transfer (call ID:%d)", call_id));
	}
    }
}

void Endpoint::on_call_transfer_status(pjsua_call_id call_id,
                                       int st_code,
                                       const pj_str_t *st_text,
                                       pj_bool_t final,
                                       pj_bool_t *p_cont)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallTransferStatusParam prm;
    prm.statusCode = (pjsip_status_code)st_code;
    prm.reason = pj2Str(*st_text);
    prm.finalNotify = PJ2BOOL(final);
    prm.cont = PJ2BOOL(*p_cont);
    
    call->onCallTransferStatus(prm);
    
    *p_cont = prm.cont;
}

void Endpoint::on_call_replace_request2(pjsua_call_id call_id,
                                        pjsip_rx_data *rdata,
                                        int *st_code,
                                        pj_str_t *st_text,
                                        pjsua_call_setting *opt)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallReplaceRequestParam prm;
    prm.rdata.fromPj(*rdata);
    prm.statusCode = (pjsip_status_code)*st_code;
    prm.reason = pj2Str(*st_text);
    prm.opt.fromPj(*opt);
    
    call->onCallReplaceRequest(prm);
    
    *st_code = prm.statusCode;
    *st_text = str2Pj(prm.reason);
    *opt = prm.opt.toPj();
}

void Endpoint::on_call_replaced(pjsua_call_id old_call_id,
                                pjsua_call_id new_call_id)
{
    Call *call = Call::lookup(old_call_id);
    if (!call) {
	return;
    }
    
    OnCallReplacedParam prm;
    prm.newCallId = new_call_id;
    prm.newCall = NULL;
    
    call->onCallReplaced(prm);

    if (prm.newCall) {
	/* Sanity checks */
	pj_assert(prm.newCall->id == new_call_id);
	pj_assert(prm.newCall->acc.getId() == call->acc.getId());
	pj_assert(pjsua_call_get_user_data(new_call_id) == prm.newCall);
    } else {
	PJ_LOG(4,(THIS_FILE,
		  "Warning: application has not created new Call instance "
		  "for call replace (old call ID:%d, new call ID: %d)",
		  old_call_id, new_call_id));
    }
}

void Endpoint::on_call_rx_offer(pjsua_call_id call_id,
                                const pjmedia_sdp_session *offer,
                                void *reserved,
                                pjsip_status_code *code,
                                pjsua_call_setting *opt)
{
    PJ_UNUSED_ARG(reserved);

    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallRxOfferParam prm;
    prm.offer.fromPj(*offer);
    prm.statusCode = *code;
    prm.opt.fromPj(*opt);
    
    call->onCallRxOffer(prm);
    
    *code = prm.statusCode;
    *opt = prm.opt.toPj();
}

void Endpoint::on_call_rx_reinvite(pjsua_call_id call_id,
                                   const pjmedia_sdp_session *offer,
                                   pjsip_rx_data *rdata,
			     	   void *reserved,
			     	   pj_bool_t *async,
                                   pjsip_status_code *code,
                                   pjsua_call_setting *opt)
{
    PJ_UNUSED_ARG(reserved);

    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallRxReinviteParam prm;
    prm.offer.fromPj(*offer);
    prm.rdata.fromPj(*rdata);
    prm.isAsync = PJ2BOOL(*async);
    prm.statusCode = *code;
    prm.opt.fromPj(*opt);
    
    call->onCallRxReinvite(prm);
    
    *async = prm.isAsync;
    *code = prm.statusCode;
    *opt = prm.opt.toPj();
}

void Endpoint::on_call_tx_offer(pjsua_call_id call_id,
				void *reserved,
				pjsua_call_setting *opt)
{
    PJ_UNUSED_ARG(reserved);

    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }

    OnCallTxOfferParam prm;
    prm.opt.fromPj(*opt);

    call->onCallTxOffer(prm);

    *opt = prm.opt.toPj();
}

pjsip_redirect_op Endpoint::on_call_redirected(pjsua_call_id call_id,
                                               const pjsip_uri *target,
                                               const pjsip_event *e)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return PJSIP_REDIRECT_STOP;
    }
    
    OnCallRedirectedParam prm;
    char uristr[PJSIP_MAX_URL_SIZE];
    int len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, target, uristr,
                              sizeof(uristr));
    if (len < 1) {
        pj_ansi_strcpy(uristr, "--URI too long--");
    }
    prm.targetUri = string(uristr);
    if (e)
        prm.e.fromPj(*e);
    else
        prm.e.type = PJSIP_EVENT_UNKNOWN;
    
    return call->onCallRedirected(prm);
}


struct PendingOnMediaTransportCallback : public PendingJob
{
    int call_id;
    OnCallMediaTransportStateParam prm;

    virtual void execute(bool is_pending)
    {
	PJ_UNUSED_ARG(is_pending);

	Call *call = Call::lookup(call_id);
	if (!call)
	    return;

	call->onCallMediaTransportState(prm);
    }
};

pj_status_t
Endpoint::on_call_media_transport_state(pjsua_call_id call_id,
                                        const pjsua_med_tp_state_info *info)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return PJ_SUCCESS;
    }

    PendingOnMediaTransportCallback *job = new PendingOnMediaTransportCallback;
    
    job->call_id = call_id;
    job->prm.medIdx = info->med_idx;
    job->prm.state = info->state;
    job->prm.status = info->status;
    job->prm.sipErrorCode = info->sip_err_code;
    
    Endpoint::instance().utilAddPendingJob(job);

    return PJ_SUCCESS;
}

struct PendingOnMediaEventCallback : public PendingJob
{
    int call_id;
    OnCallMediaEventParam prm;

    virtual void execute(bool is_pending)
    {
	if (is_pending) {
	    /* Can't do this anymore, pointer is invalid */
	    prm.ev.pjMediaEvent = NULL;
	}

	if (call_id == PJSUA_INVALID_ID) {
	    OnMediaEventParam prm2;
	    prm2.ev = prm.ev;
	    Endpoint::instance().onMediaEvent(prm2);
	} else {
	    Call *call = Call::lookup(call_id);
	    
	    if (call)
		call->onCallMediaEvent(prm);
	}
    }
};

void Endpoint::on_media_event(pjmedia_event *event)
{
    PendingOnMediaEventCallback *job = new PendingOnMediaEventCallback;

    job->call_id = PJSUA_INVALID_ID;
    job->prm.medIdx = 0;
    job->prm.ev.fromPj(*event);
    
    Endpoint::instance().utilAddPendingJob(job);
}

void Endpoint::on_call_media_event(pjsua_call_id call_id,
                                   unsigned med_idx,
                                   pjmedia_event *event)
{
    PendingOnMediaEventCallback *job = new PendingOnMediaEventCallback;

    job->call_id = call_id;
    job->prm.medIdx = med_idx;
    job->prm.ev.fromPj(*event);
    
    Endpoint::instance().utilAddPendingJob(job);
}

pjmedia_transport*
Endpoint::on_create_media_transport(pjsua_call_id call_id,
                                    unsigned media_idx,
                                    pjmedia_transport *base_tp,
                                    unsigned flags)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	pjsua_call *in_call = &pjsua_var.calls[call_id];
	if (in_call->incoming_data) {
	    /* This can happen when there is an incoming call but the
	     * on_incoming_call() callback hasn't been called. So we need to 
	     * call the callback here.
	     */
	    on_incoming_call(in_call->acc_id, call_id, in_call->incoming_data);

	    /* New call should already be created by app. */
	    call = Call::lookup(call_id);
	    if (!call) {
		return base_tp;
	    }
	    if (in_call->inv->dlg->mod_data[pjsua_var.mod.id] == NULL) {
		/* This will enabled notification for fail events related to 
		 * the call via on_call_state() and on_call_tsx_state().
		 */
		in_call->inv->dlg->mod_data[pjsua_var.mod.id] = in_call;
		in_call->inv->mod_data[pjsua_var.mod.id] = in_call;
		++pjsua_var.call_cnt;
	    }
	} else {
	    return base_tp;
	}
    }
    
    OnCreateMediaTransportParam prm;
    prm.mediaIdx = media_idx;
    prm.mediaTp = base_tp;
    prm.flags = flags;
    
    call->onCreateMediaTransport(prm);
    
    return (pjmedia_transport *)prm.mediaTp;
}

void Endpoint::on_create_media_transport_srtp(pjsua_call_id call_id,
                                    	      unsigned media_idx,
                                    	      pjmedia_srtp_setting *srtp_opt)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	pjsua_call *in_call = &pjsua_var.calls[call_id];
	if (in_call->incoming_data) {
	    /* This can happen when there is an incoming call but the
	     * on_incoming_call() callback hasn't been called. So we need to 
	     * call the callback here.
	     */
	    on_incoming_call(in_call->acc_id, call_id, in_call->incoming_data);

	    /* New call should already be created by app. */
	    call = Call::lookup(call_id);
	    if (!call) {
		return;
	    }
	} else {
	    return;
	}
    }
    
    OnCreateMediaTransportSrtpParam prm;
    prm.mediaIdx = media_idx;
    prm.srtpUse  = srtp_opt->use;
    for (unsigned i = 0; i < srtp_opt->crypto_count; i++) {
    	SrtpCrypto crypto;
    	
    	crypto.key   = pj2Str(srtp_opt->crypto[i].key);
    	crypto.name  = pj2Str(srtp_opt->crypto[i].name);
    	crypto.flags = srtp_opt->crypto[i].flags;
    	prm.cryptos.push_back(crypto);
    }
    
    call->onCreateMediaTransportSrtp(prm);
    
    srtp_opt->use = prm.srtpUse;
    srtp_opt->crypto_count = (unsigned)prm.cryptos.size();
    for (unsigned i = 0; i < srtp_opt->crypto_count; i++) {
    	srtp_opt->crypto[i].key   = str2Pj(prm.cryptos[i].key);
    	srtp_opt->crypto[i].name  = str2Pj(prm.cryptos[i].name);
    	srtp_opt->crypto[i].flags = prm.cryptos[i].flags;
    }
}

void Endpoint::on_ip_change_progress(pjsua_ip_change_op op,
				     pj_status_t status,
				     const pjsua_ip_change_op_info *info)
{
    Endpoint &ep = Endpoint::instance();
    OnIpChangeProgressParam param;

    param.op = op;
    param.status = status;
    switch (op) {
    case PJSUA_IP_CHANGE_OP_RESTART_LIS:		
	param.transportId = info->lis_restart.transport_id;	
	break;
    case PJSUA_IP_CHANGE_OP_ACC_SHUTDOWN_TP:
	param.accId = info->acc_shutdown_tp.acc_id;
	break;
    case PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT:	
	param.accId = info->acc_update_contact.acc_id;	
	param.regInfo.code = info->acc_update_contact.code;
	param.regInfo.isRegister = 
				 PJ2BOOL(info->acc_update_contact.is_register);
	break;
    case PJSUA_IP_CHANGE_OP_ACC_HANGUP_CALLS:
	param.accId = info->acc_hangup_calls.acc_id;
	param.callId = info->acc_hangup_calls.call_id;
	break;
    case PJSUA_IP_CHANGE_OP_ACC_REINVITE_CALLS:
	param.accId = info->acc_reinvite_calls.acc_id;
	param.callId = info->acc_reinvite_calls.call_id;
	break;
    default:
        param.accId = PJSUA_INVALID_ID;
        break;
    }
    ep.onIpChangeProgress(param);
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint library operations
 */
Version Endpoint::libVersion() const
{
    Version ver;
    ver.major = PJ_VERSION_NUM_MAJOR;
    ver.minor = PJ_VERSION_NUM_MINOR;
    ver.rev = PJ_VERSION_NUM_REV;
    ver.suffix = PJ_VERSION_NUM_EXTRA;
    ver.full = pj_get_version();
    ver.numeric = PJ_VERSION_NUM;
    return ver;
}

void Endpoint::libCreate() PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_create() );
    mainThread = pj_thread_this();
    
    /* Register library main thread */
    threadDescMap[pj_thread_this()] = NULL;
}

pjsua_state Endpoint::libGetState() const
{
    return pjsua_get_state();
}

void Endpoint::libInit(const EpConfig &prmEpConfig) PJSUA2_THROW(Error)
{
    pjsua_config ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config med_cfg;

    ua_cfg = prmEpConfig.uaConfig.toPj();
    log_cfg = prmEpConfig.logConfig.toPj();
    med_cfg = prmEpConfig.medConfig.toPj();

    /* Setup log callback */
    if (prmEpConfig.logConfig.writer) {
	this->writer = prmEpConfig.logConfig.writer;
	log_cfg.cb = &Endpoint::logFunc;
    }
    mainThreadOnly = prmEpConfig.uaConfig.mainThreadOnly;

    /* Setup UA callbacks */
    pj_bzero(&ua_cfg.cb, sizeof(ua_cfg.cb));
    ua_cfg.cb.on_nat_detect 	= &Endpoint::on_nat_detect;
    ua_cfg.cb.on_transport_state = &Endpoint::on_transport_state;

    ua_cfg.cb.on_incoming_call	= &Endpoint::on_incoming_call;
    ua_cfg.cb.on_reg_started	= &Endpoint::on_reg_started;
    ua_cfg.cb.on_reg_state2	= &Endpoint::on_reg_state2;
    ua_cfg.cb.on_incoming_subscribe = &Endpoint::on_incoming_subscribe;
    ua_cfg.cb.on_pager2		= &Endpoint::on_pager2;
    ua_cfg.cb.on_pager_status2	= &Endpoint::on_pager_status2;
    ua_cfg.cb.on_typing2	= &Endpoint::on_typing2;
    ua_cfg.cb.on_mwi_info	= &Endpoint::on_mwi_info;
    ua_cfg.cb.on_buddy_state	= &Endpoint::on_buddy_state;
    ua_cfg.cb.on_buddy_evsub_state = &Endpoint::on_buddy_evsub_state;
    ua_cfg.cb.on_acc_find_for_incoming  = &Endpoint::on_acc_find_for_incoming;
    ua_cfg.cb.on_ip_change_progress	= &Endpoint::on_ip_change_progress;

    /* Call callbacks */
    ua_cfg.cb.on_call_state             = &Endpoint::on_call_state;
    ua_cfg.cb.on_call_tsx_state         = &Endpoint::on_call_tsx_state;
    ua_cfg.cb.on_call_media_state       = &Endpoint::on_call_media_state;
    ua_cfg.cb.on_call_sdp_created       = &Endpoint::on_call_sdp_created;
    ua_cfg.cb.on_stream_created2        = &Endpoint::on_stream_created2;
    ua_cfg.cb.on_stream_destroyed       = &Endpoint::on_stream_destroyed;
    //ua_cfg.cb.on_dtmf_digit             = &Endpoint::on_dtmf_digit;
    //ua_cfg.cb.on_dtmf_digit2            = &Endpoint::on_dtmf_digit2;
    ua_cfg.cb.on_dtmf_event             = &Endpoint::on_dtmf_event;
    ua_cfg.cb.on_call_transfer_request2 = &Endpoint::on_call_transfer_request2;
    ua_cfg.cb.on_call_transfer_status   = &Endpoint::on_call_transfer_status;
    ua_cfg.cb.on_call_replace_request2  = &Endpoint::on_call_replace_request2;
    ua_cfg.cb.on_call_replaced          = &Endpoint::on_call_replaced;
    ua_cfg.cb.on_call_rx_offer          = &Endpoint::on_call_rx_offer;
    ua_cfg.cb.on_call_rx_reinvite       = &Endpoint::on_call_rx_reinvite;
    ua_cfg.cb.on_call_tx_offer          = &Endpoint::on_call_tx_offer;
    ua_cfg.cb.on_call_redirected        = &Endpoint::on_call_redirected;
    ua_cfg.cb.on_call_media_transport_state =
        &Endpoint::on_call_media_transport_state;
    ua_cfg.cb.on_media_event		= &Endpoint::on_media_event;
    ua_cfg.cb.on_call_media_event       = &Endpoint::on_call_media_event;
    ua_cfg.cb.on_create_media_transport = &Endpoint::on_create_media_transport;
    ua_cfg.cb.on_stun_resolution_complete = 
    	&Endpoint::stun_resolve_cb;

    /* Init! */
    PJSUA2_CHECK_EXPR( pjsua_init(&ua_cfg, &log_cfg, &med_cfg) );

    /* Register worker threads */
    int i = pjsua_var.ua_cfg.thread_cnt;
    while (i) {
	pj_thread_t *t = pjsua_var.thread[--i];
	if (t)
	    threadDescMap[t] = NULL;
    }

    /* Register media endpoint worker thread */
    pjmedia_endpt *medept = pjsua_get_pjmedia_endpt();
    i = pjmedia_endpt_get_thread_count(medept);
    while (i) {
	pj_thread_t *t = pjmedia_endpt_get_thread(medept, --i);
	if (t)
	    threadDescMap[t] = NULL;
    }
    
    PJSUA2_CHECK_EXPR( pj_mutex_create_simple(pjsua_var.pool, "threadDesc",
    				    	      &threadDescMutex) );

#if !DEPRECATED_FOR_TICKET_2232
    PJSUA2_CHECK_EXPR( pj_mutex_create_recursive(pjsua_var.pool, "mediaList",
    				    		 &mediaListMutex) );
#endif
}

void Endpoint::libStart() PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR(pjsua_start());
}

void Endpoint::libRegisterThread(const string &name) PJSUA2_THROW(Error)
{
    pj_thread_t *thread;
    pj_thread_desc *desc;
    pj_status_t status;

    desc = (pj_thread_desc*)malloc(sizeof(pj_thread_desc));
    if (!desc) {
	PJSUA2_RAISE_ERROR(PJ_ENOMEM);
    }

    pj_bzero(desc, sizeof(pj_thread_desc));

    status = pj_thread_register(name.c_str(), *desc, &thread);
    if (status == PJ_SUCCESS) {
    	pj_mutex_lock(threadDescMutex);
	threadDescMap[thread] = desc;
	pj_mutex_unlock(threadDescMutex);
    } else {
	free(desc);
	PJSUA2_RAISE_ERROR(status);
    }
}

bool Endpoint::libIsThreadRegistered()
{
    if (pj_thread_is_registered()) {
    	bool found;

    	pj_mutex_lock(threadDescMutex);
	/* Recheck again if it exists in the thread description map */
	found = (threadDescMap.find(pj_thread_this()) != threadDescMap.end());
	pj_mutex_unlock(threadDescMutex);

	return found;
    }

    return false;
}

void Endpoint::libStopWorkerThreads()
{
    pjsua_stop_worker_threads();
}

int Endpoint::libHandleEvents(unsigned msec_timeout)
{
    performPendingJobs();
    return pjsua_handle_events(msec_timeout);
}

void Endpoint::libDestroy(unsigned flags) PJSUA2_THROW(Error)
{
    pj_status_t status;

    if (threadDescMutex) {
    	pj_mutex_destroy(threadDescMutex);
    	threadDescMutex = NULL;
    }

#if !DEPRECATED_FOR_TICKET_2232
    while(mediaList.size() > 0) {
	AudioMedia *cur_media = mediaList[0];
	delete cur_media; /* this will remove itself from the list */
    }

    if (mediaListMutex) {
    	pj_mutex_destroy(mediaListMutex);
    	mediaListMutex = NULL;
    }
#endif

    status = pjsua_destroy2(flags);

    delete this->writer;
    this->writer = NULL;

#if PJ_LOG_MAX_LEVEL >= 1
    if (pj_log_get_log_func() == &Endpoint::logFunc) {
	pj_log_set_log_func(NULL);
    }
#endif

    /* Clean up thread descriptors */
    std::map<pj_thread_t*, pj_thread_desc*>::iterator i;
    for (i = threadDescMap.begin(); i != threadDescMap.end(); ++i) {
	pj_thread_desc* d = (*i).second;
	if (d != NULL)
	    free(d);
    }
    threadDescMap.clear();

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
#if PJ_LOG_MAX_LEVEL >= 1
    va_list arg;
    va_start(arg, format);
    pj_log(sender, level, format, arg );
    va_end(arg);
#endif
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
                                  Token prmUserData) PJSUA2_THROW(Error)
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

IntVector Endpoint::utilSslGetAvailableCiphers() PJSUA2_THROW(Error)
{
#if PJ_HAS_SSL_SOCK
    pj_ssl_cipher ciphers[PJ_SSL_SOCK_MAX_CIPHERS];
    unsigned count = PJ_ARRAY_SIZE(ciphers);

    PJSUA2_CHECK_EXPR( pj_ssl_cipher_get_availables(ciphers, &count) );

    return IntVector(ciphers, ciphers + count);
#else
    return IntVector();
#endif
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint NAT operations
 */
void Endpoint::natDetectType(void) PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_detect_nat_type() );
}

pj_stun_nat_type Endpoint::natGetType() PJSUA2_THROW(Error)
{
    pj_stun_nat_type type;

    PJSUA2_CHECK_EXPR( pjsua_get_nat_type(&type) );

    return type;
}

void Endpoint::natUpdateStunServers(const StringVector &servers,
				    bool wait) PJSUA2_THROW(Error)
{
    pj_str_t srv[MAX_STUN_SERVERS];
    unsigned i, count = 0;

    for (i=0; i<servers.size() && i<MAX_STUN_SERVERS; ++i) {
	srv[count].ptr = (char*)servers[i].c_str();
	srv[count].slen = servers[i].size();
	++count;
    }

    PJSUA2_CHECK_EXPR(pjsua_update_stun_servers(count, srv, wait) );
}

void Endpoint::natCheckStunServers(const StringVector &servers,
				   bool wait,
				   Token token) PJSUA2_THROW(Error)
{
    pj_str_t srv[MAX_STUN_SERVERS];
    unsigned i, count = 0;

    for (i=0; i<servers.size() && i<MAX_STUN_SERVERS; ++i) {
	srv[count].ptr = (char*)servers[i].c_str();
	srv[count].slen = servers[i].size();
	++count;
    }

    PJSUA2_CHECK_EXPR(pjsua_resolve_stun_servers(count, srv, wait, token,
                                                 &Endpoint::stun_resolve_cb) );
}

void Endpoint::natCancelCheckStunServers(Token token,
                                         bool notify_cb) PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_cancel_stun_resolution(token, notify_cb) );
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Transport API
 */
TransportId Endpoint::transportCreate(pjsip_transport_type_e type,
                                      const TransportConfig &cfg)
				      PJSUA2_THROW(Error)
{
    pjsua_transport_config tcfg;
    pjsua_transport_id tid;

    tcfg = cfg.toPj();
    PJSUA2_CHECK_EXPR( pjsua_transport_create(type,
                                              &tcfg, &tid) );

    return tid;
}

IntVector Endpoint::transportEnum() PJSUA2_THROW(Error)
{
    pjsua_transport_id tids[32];
    unsigned count = PJ_ARRAY_SIZE(tids);

    PJSUA2_CHECK_EXPR( pjsua_enum_transports(tids, &count) );

    return IntVector(tids, tids+count);
}

TransportInfo Endpoint::transportGetInfo(TransportId id) PJSUA2_THROW(Error)
{
    pjsua_transport_info pj_tinfo;
    TransportInfo tinfo;

    PJSUA2_CHECK_EXPR( pjsua_transport_get_info(id, &pj_tinfo) );
    tinfo.fromPj(pj_tinfo);

    return tinfo;
}

void Endpoint::transportSetEnable(TransportId id, bool enabled)
				  PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_transport_set_enable(id, enabled) );
}

void Endpoint::transportClose(TransportId id) PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_transport_close(id, PJ_FALSE) );
}

void Endpoint::transportShutdown(TransportHandle tp) PJSUA2_THROW(Error)
{
    PJSUA2_CHECK_EXPR( pjsip_transport_shutdown((pjsip_transport *)tp) );
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Call operations
 */

void Endpoint::hangupAllCalls(void)
{
    pjsua_call_hangup_all();
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Media API
 */
unsigned Endpoint::mediaMaxPorts() const
{
    return pjsua_conf_get_max_ports();
}

unsigned Endpoint::mediaActivePorts() const
{
    return pjsua_conf_get_active_ports();
}

#if !DEPRECATED_FOR_TICKET_2232
const AudioMediaVector &Endpoint::mediaEnumPorts() const PJSUA2_THROW(Error)
{
    return mediaList;
}
#endif

AudioMediaVector2 Endpoint::mediaEnumPorts2() const PJSUA2_THROW(Error)
{
    AudioMediaVector2 amv2;
    pjsua_conf_port_id ids[PJSUA_MAX_CONF_PORTS];
    unsigned i, count = PJSUA_MAX_CONF_PORTS;

    PJSUA2_CHECK_EXPR( pjsua_enum_conf_ports(ids, &count) );
    for (i = 0; i < count; ++i) {
	AudioMediaHelper am;
	am.setPortId(ids[i]);
	amv2.push_back(am);
    }

    return amv2;
}

VideoMediaVector Endpoint::mediaEnumVidPorts() const PJSUA2_THROW(Error)
{
#if PJSUA_HAS_VIDEO
    VideoMediaVector vmv;
    pjsua_conf_port_id ids[PJSUA_MAX_CONF_PORTS];
    unsigned i, count = PJSUA_MAX_CONF_PORTS;

    PJSUA2_CHECK_EXPR( pjsua_vid_conf_enum_ports(ids, &count) );
    for (i = 0; i < count; ++i) {
	VideoMediaHelper vm;
	vm.setPortId(ids[i]);
	vmv.push_back(vm);
    }

    return vmv;
#else
    PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
#endif
}

void Endpoint::mediaAdd(AudioMedia &media)
{
#if !DEPRECATED_FOR_TICKET_2232
    /* mediaList serves mediaEnumPorts() only, once mediaEnumPorts()
     * is removed, this function implementation should be no-op.
     */
    pj_mutex_lock(mediaListMutex);

    AudioMediaVector::iterator it = std::find(mediaList.begin(),
					      mediaList.end(),
					      &media);

    if (it == mediaList.end())
    	mediaList.push_back(&media);
    pj_mutex_unlock(mediaListMutex);
#else
    PJ_UNUSED_ARG(media);
#endif
}

void Endpoint::mediaRemove(AudioMedia &media)
{
#if !DEPRECATED_FOR_TICKET_2232
    /* mediaList serves mediaEnumPorts() only, once mediaEnumPorts()
     * is removed, this function implementation should be no-op.
     */
    pj_mutex_lock(mediaListMutex);
    AudioMediaVector::iterator it = std::find(mediaList.begin(),
					      mediaList.end(),
					      &media);

    if (it != mediaList.end())
	mediaList.erase(it);
    pj_mutex_unlock(mediaListMutex);
#else
    PJ_UNUSED_ARG(media);
#endif
}

bool Endpoint::mediaExists(const AudioMedia &media) const
{
    pjsua_conf_port_id id = media.getPortId();
    if (id == PJSUA_INVALID_ID || id >= (int)mediaMaxPorts())
	return false;

    pjsua_conf_port_info pi;
    return (pjsua_conf_get_port_info(id, &pi) == PJ_SUCCESS);
}

AudDevManager &Endpoint::audDevManager()
{
    return audioDevMgr;
}

VidDevManager &Endpoint::vidDevManager()
{
    return videoDevMgr;
}

/*
 * Codec operations.
 */

#if !DEPRECATED_FOR_TICKET_2232
const CodecInfoVector &Endpoint::codecEnum() PJSUA2_THROW(Error)
{
    pjsua_codec_info pj_codec[MAX_CODEC_NUM];
    unsigned count = MAX_CODEC_NUM;

    PJSUA2_CHECK_EXPR( pjsua_enum_codecs(pj_codec, &count) );

    updateCodecInfoList(pj_codec, count, codecInfoList);
    return codecInfoList;
}
#endif

CodecInfoVector2 Endpoint::codecEnum2() const PJSUA2_THROW(Error)
{
    CodecInfoVector2 civ2;
    pjsua_codec_info pj_codec[MAX_CODEC_NUM];
    unsigned count = MAX_CODEC_NUM;

    PJSUA2_CHECK_EXPR( pjsua_enum_codecs(pj_codec, &count) );
    for (unsigned i = 0; i<count; ++i) {
	CodecInfo codec_info;
	codec_info.fromPj(pj_codec[i]);
	civ2.push_back(codec_info);
    }
    return civ2;
}

void Endpoint::codecSetPriority(const string &codec_id,
			        pj_uint8_t priority) PJSUA2_THROW(Error)
{
    pj_str_t codec_str = str2Pj(codec_id);
    PJSUA2_CHECK_EXPR( pjsua_codec_set_priority(&codec_str, priority) );
}

CodecParam Endpoint::codecGetParam(const string &codec_id) const
				   PJSUA2_THROW(Error)
{
    CodecParam param;
    pjmedia_codec_param pj_param;
    pj_str_t codec_str = str2Pj(codec_id);

    PJSUA2_CHECK_EXPR( pjsua_codec_get_param(&codec_str, &pj_param) );

    param.fromPj(pj_param);
    return param;
}

void Endpoint::codecSetParam(const string &codec_id,
			     const CodecParam param) PJSUA2_THROW(Error)
{
    pj_str_t codec_str = str2Pj(codec_id);
    pjmedia_codec_param pj_param = param.toPj();

    PJSUA2_CHECK_EXPR( pjsua_codec_set_param(&codec_str, &pj_param) );
}

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC!=0)

CodecOpusConfig Endpoint::getCodecOpusConfig() const PJSUA2_THROW(Error)
{
   pjmedia_codec_opus_config opus_cfg;
   CodecOpusConfig config;

   PJSUA2_CHECK_EXPR(pjmedia_codec_opus_get_config(&opus_cfg));
   config.fromPj(opus_cfg);

   return config;
}

void Endpoint::setCodecOpusConfig(const CodecOpusConfig &opus_cfg)
				  PJSUA2_THROW(Error)
{
   const pj_str_t codec_id = {(char *)"opus", 4};
   pjmedia_codec_param param;
   pjmedia_codec_opus_config new_opus_cfg;

   PJSUA2_CHECK_EXPR(pjsua_codec_get_param(&codec_id, &param));

   PJSUA2_CHECK_EXPR(pjmedia_codec_opus_get_config(&new_opus_cfg));

   new_opus_cfg = opus_cfg.toPj();

   PJSUA2_CHECK_EXPR(pjmedia_codec_opus_set_default_param(&new_opus_cfg,
							  &param));
}

#endif

void Endpoint::clearCodecInfoList(CodecInfoVector &codec_list)
{
    for (unsigned i=0;i<codec_list.size();++i) {
	delete codec_list[i];
    }
    codec_list.clear();
}

void Endpoint::updateCodecInfoList(pjsua_codec_info pj_codec[],
				   unsigned count,
				   CodecInfoVector &codec_list)
{
    pj_enter_critical_section();
    clearCodecInfoList(codec_list);
    for (unsigned i = 0; i<count; ++i) {
	CodecInfo *codec_info = new CodecInfo;

	codec_info->fromPj(pj_codec[i]);
	codec_list.push_back(codec_info);
    }
    pj_leave_critical_section();
}

#if !DEPRECATED_FOR_TICKET_2232
const CodecInfoVector &Endpoint::videoCodecEnum() PJSUA2_THROW(Error)
{
#if PJSUA_HAS_VIDEO
    pjsua_codec_info pj_codec[MAX_CODEC_NUM];
    unsigned count = MAX_CODEC_NUM;

    PJSUA2_CHECK_EXPR(pjsua_vid_enum_codecs(pj_codec, &count));

    updateCodecInfoList(pj_codec, count, videoCodecInfoList);
#endif
    return videoCodecInfoList;
}
#endif

CodecInfoVector2 Endpoint::videoCodecEnum2() const PJSUA2_THROW(Error)
{
    CodecInfoVector2 civ2;
#if PJSUA_HAS_VIDEO
    pjsua_codec_info pj_codec[MAX_CODEC_NUM];
    unsigned count = MAX_CODEC_NUM;

    PJSUA2_CHECK_EXPR(pjsua_vid_enum_codecs(pj_codec, &count));
    for (unsigned i = 0; i<count; ++i) {
	CodecInfo codec_info;
	codec_info.fromPj(pj_codec[i]);
	civ2.push_back(codec_info);
    }
#endif
    return civ2;
}

void Endpoint::videoCodecSetPriority(const string &codec_id,
				     pj_uint8_t priority) PJSUA2_THROW(Error)
{
#if PJSUA_HAS_VIDEO
    pj_str_t codec_str = str2Pj(codec_id);
    PJSUA2_CHECK_EXPR(pjsua_vid_codec_set_priority(&codec_str, priority));
#else
    PJ_UNUSED_ARG(codec_id);
    PJ_UNUSED_ARG(priority);
#endif
}

VidCodecParam Endpoint::getVideoCodecParam(const string &codec_id) const 
					   PJSUA2_THROW(Error)
{    
    VidCodecParam codec_param;
#if PJSUA_HAS_VIDEO
    pjmedia_vid_codec_param pj_param;
    pj_str_t codec_str = str2Pj(codec_id);    

    PJSUA2_CHECK_EXPR(pjsua_vid_codec_get_param(&codec_str, &pj_param));
    codec_param.fromPj(pj_param);
#else
    PJ_UNUSED_ARG(codec_id);
#endif
    return codec_param;
}

void Endpoint::setVideoCodecParam(const string &codec_id,
				  const VidCodecParam &param)
				  PJSUA2_THROW(Error)
{
#if PJSUA_HAS_VIDEO
    pj_str_t codec_str = str2Pj(codec_id);
    pjmedia_vid_codec_param pj_param = param.toPj();
    
    PJSUA2_CHECK_EXPR(pjsua_vid_codec_set_param(&codec_str, &pj_param));
#else
    PJ_UNUSED_ARG(codec_id);
    PJ_UNUSED_ARG(param);
#endif
}

void Endpoint::resetVideoCodecParam(const string &codec_id)
				    PJSUA2_THROW(Error)
{
#if PJSUA_HAS_VIDEO
    pj_str_t codec_str = str2Pj(codec_id);    
    
    PJSUA2_CHECK_EXPR(pjsua_vid_codec_set_param(&codec_str, NULL));
#else
    PJ_UNUSED_ARG(codec_id);    
#endif	
}

/*
 * Enumerate all SRTP crypto-suite names.
 */
StringVector Endpoint::srtpCryptoEnum() PJSUA2_THROW(Error)
{
    StringVector result;
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    unsigned cnt = PJMEDIA_SRTP_MAX_CRYPTOS;
    pjmedia_srtp_crypto cryptos[PJMEDIA_SRTP_MAX_CRYPTOS];

    PJSUA2_CHECK_EXPR(pjmedia_srtp_enum_crypto(&cnt, cryptos));

    for (unsigned i = 0; i < cnt; ++i)
	result.push_back(pj2Str(cryptos[i].name));
#endif

    return result;
}

void Endpoint::handleIpChange(const IpChangeParam &param) PJSUA2_THROW(Error)
{
    pjsua_ip_change_param ip_change_param = param.toPj();
    PJSUA2_CHECK_EXPR(pjsua_handle_ip_change(&ip_change_param));
}
