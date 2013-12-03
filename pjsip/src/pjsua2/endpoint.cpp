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
#include <pjsua2/presence.hpp>
#include <algorithm>
#include "util.hpp"

using namespace pj;
using namespace std;

#include <pjsua2/account.hpp>
#include <pjsua2/call.hpp>

#define THIS_FILE		"endpoint.cpp"
#define MAX_STUN_SERVERS	32
#define TIMER_SIGNATURE		0x600D878A

struct UserTimer
{
    pj_uint32_t		signature;
    OnTimerParam	prm;
    pj_timer_entry	entry;
};

Endpoint *Endpoint::instance_;

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

    pua_cfg.nat_type_in_sdp = this->natTypeInSdp;
    pua_cfg.enable_unsolicited_mwi = this->mwiUnsolicitedEnabled;

    return pua_cfg;
}

void UaConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("UaConfig");

    NODE_READ_UNSIGNED( this_node, maxCalls);
    NODE_READ_UNSIGNED( this_node, threadCnt);
    NODE_READ_STRINGV ( this_node, nameserver);
    NODE_READ_STRING  ( this_node, userAgent);
    NODE_READ_STRINGV ( this_node, stunServer);
    NODE_READ_BOOL    ( this_node, stunIgnoreFailure);
    NODE_READ_INT     ( this_node, natTypeInSdp);
    NODE_READ_BOOL    ( this_node, mwiUnsolicitedEnabled);
}

void UaConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("UaConfig");

    NODE_WRITE_UNSIGNED( this_node, maxCalls);
    NODE_WRITE_UNSIGNED( this_node, threadCnt);
    NODE_WRITE_STRINGV ( this_node, nameserver);
    NODE_WRITE_STRING  ( this_node, userAgent);
    NODE_WRITE_STRINGV ( this_node, stunServer);
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

void LogConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("LogConfig");

    NODE_READ_UNSIGNED( this_node, msgLogging);
    NODE_READ_UNSIGNED( this_node, level);
    NODE_READ_UNSIGNED( this_node, consoleLevel);
    NODE_READ_UNSIGNED( this_node, decor);
    NODE_READ_STRING  ( this_node, filename);
    NODE_READ_UNSIGNED( this_node, fileFlags);
}

void LogConfig::writeObject(ContainerNode &node) const throw(Error)
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
    mcfg.snd_auto_close_time = this->sndAutoCloseTime;
    mcfg.vid_preview_enable_native = this->vidPreviewEnableNative;

    return mcfg;
}

void MediaConfig::readObject(const ContainerNode &node) throw(Error)
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
    NODE_READ_INT     ( this_node, sndAutoCloseTime);
    NODE_READ_BOOL    ( this_node, vidPreviewEnableNative);
}

void MediaConfig::writeObject(ContainerNode &node) const throw(Error)
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
    NODE_WRITE_INT     ( this_node, sndAutoCloseTime);
    NODE_WRITE_BOOL    ( this_node, vidPreviewEnableNative);
}

///////////////////////////////////////////////////////////////////////////////

void EpConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("EpConfig");
    NODE_READ_OBJ( this_node, uaConfig);
    NODE_READ_OBJ( this_node, logConfig);
    NODE_READ_OBJ( this_node, medConfig);
}

void EpConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("EpConfig");
    NODE_WRITE_OBJ( this_node, uaConfig);
    NODE_WRITE_OBJ( this_node, logConfig);
    NODE_WRITE_OBJ( this_node, medConfig);
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Endpoint instance
 */
Endpoint::Endpoint()
: writer(NULL)
{
    if (instance_) {
	PJSUA2_RAISE_ERROR(PJ_EEXISTS);
    }

    instance_ = this;
}

Endpoint& Endpoint::instance() throw(Error)
{
    if (!instance_) {
	PJSUA2_RAISE_ERROR(PJ_ENOTFOUND);
    }
    return *instance_;
}

Endpoint::~Endpoint()
{
    try {
	libDestroy();
    } catch (Error &err) {
	// Ignore
	PJ_UNUSED_ARG(err);
    }
    delete writer;
    
    while(mediaList.size() > 0) {
	AudioMedia *cur_media = mediaList[0];
	delete cur_media;
    }

    instance_ = NULL;
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
    prm.state = state;
    prm.lastError = info ? info->status : PJ_SUCCESS;

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

void Endpoint::on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                                pjsip_rx_data *rdata)
{
    Account *acc = lookupAcc(acc_id, "on_incoming_call()");
    if (!acc) {
	pjsua_call_hangup(call_id, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL);
	return;
    }

    /* call callback */
    OnIncomingCallParam prm;
    prm.callId = call_id;
    prm.rdata.fromPj(*rdata);

    acc->onIncomingCall(prm);

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
	// TODO:
	//	handle call pager
	return;
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
	// TODO:
	//	handle call pager
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
	// TODO:
	//	handle call indication
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
    prm.state	= pjsip_evsub_get_state(mwi_info->evsub);
    prm.rdata.fromPj(*mwi_info->rdata);

    Account *acc = lookupAcc(acc_id, "on_mwi_info()");
    if (!acc) {
	/* Ignored */
	return;
    }

    acc->onMwiInfo(prm);
}

void Endpoint::on_buddy_state(pjsua_buddy_id buddy_id)
{
    Buddy *buddy = (Buddy*)pjsua_buddy_get_user_data(buddy_id);
    if (!buddy || !buddy->isValid()) {
	/* Ignored */
	return;
    }

    buddy->onBuddyState();
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
    
    call->onCallSdpCreated(prm);
    
    /* Check if application modifies the SDP */
    if (orig_sdp != prm.sdp.wholeSdp) {
        pjmedia_sdp_parse(pool, (char*)prm.sdp.wholeSdp.c_str(),
                          prm.sdp.wholeSdp.size(), &sdp);
    }
}

void Endpoint::on_stream_created(pjsua_call_id call_id,
                                 pjmedia_stream *strm,
                                 unsigned stream_idx,
                                 pjmedia_port **p_port)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnStreamCreatedParam prm;
    prm.stream = strm;
    prm.streamIdx = stream_idx;
    prm.pPort = (void *)*p_port;
    
    call->onStreamCreated(prm);
    
    if (prm.pPort != (void *)*p_port)
        *p_port = (pjmedia_port *)prm.pPort;
}

void Endpoint::on_stream_destroyed(pjsua_call_id call_id,
                                   pjmedia_stream *strm,
                                   unsigned stream_idx)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnStreamDestroyedParam prm;
    prm.stream = strm;
    prm.streamIdx = stream_idx;
    
    call->onStreamDestroyed(prm);
}

void Endpoint::on_dtmf_digit(pjsua_call_id call_id, int digit)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnDtmfDigitParam prm;
    char buf[10];
    pj_ansi_sprintf(buf, "%c", digit);
    prm.digit = (string)buf;
    
    call->onDtmfDigit(prm);
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
    
    call->onCallTransferRequest(prm);
    
    *code = prm.statusCode;
    *opt = prm.opt.toPj();
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
    prm.finalNotify = final;
    prm.cont = *p_cont;
    
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
    
    call->onCallReplaced(prm);
}

void Endpoint::on_call_rx_offer(pjsua_call_id call_id,
                                const pjmedia_sdp_session *offer,
                                void *reserved,
                                pjsip_status_code *code,
                                pjsua_call_setting *opt)
{
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

pj_status_t
Endpoint::on_call_media_transport_state(pjsua_call_id call_id,
                                        const pjsua_med_tp_state_info *info)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return PJ_SUCCESS;
    }
    
    OnCallMediaTransportStateParam prm;
    prm.medIdx = info->med_idx;
    prm.state = info->state;
    prm.status = info->status;
    prm.sipErrorCode = info->sip_err_code;
    
    call->onCallMediaTransportState(prm);
    
    return PJ_SUCCESS;
}

void Endpoint::on_call_media_event(pjsua_call_id call_id,
                                   unsigned med_idx,
                                   pjmedia_event *event)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return;
    }
    
    OnCallMediaEventParam prm;
    prm.medIdx = med_idx;
    prm.ev.fromPj(*event);
    
    call->onCallMediaEvent(prm);
}

pjmedia_transport*
Endpoint::on_create_media_transport(pjsua_call_id call_id,
                                    unsigned media_idx,
                                    pjmedia_transport *base_tp,
                                    unsigned flags)
{
    Call *call = Call::lookup(call_id);
    if (!call) {
	return base_tp;
    }
    
    OnCreateMediaTransportParam prm;
    prm.mediaIdx = media_idx;
    prm.mediaTp = base_tp;
    prm.flags = flags;
    
    call->onCreateMediaTransport(prm);
    
    return (pjmedia_transport *)prm.mediaTp;
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

void Endpoint::libCreate() throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_create() );
}

pjsua_state Endpoint::libGetState() const
{
    return pjsua_get_state();
}

void Endpoint::libInit(const EpConfig &prmEpConfig) throw(Error)
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

    /* Call callbacks */
    ua_cfg.cb.on_call_state             = &Endpoint::on_call_state;
    ua_cfg.cb.on_call_tsx_state         = &Endpoint::on_call_tsx_state;
    ua_cfg.cb.on_call_media_state       = &Endpoint::on_call_media_state;
    ua_cfg.cb.on_call_sdp_created       = &Endpoint::on_call_sdp_created;
    ua_cfg.cb.on_stream_created         = &Endpoint::on_stream_created;
    ua_cfg.cb.on_stream_destroyed       = &Endpoint::on_stream_destroyed;
    ua_cfg.cb.on_dtmf_digit             = &Endpoint::on_dtmf_digit;
    ua_cfg.cb.on_call_transfer_request2 = &Endpoint::on_call_transfer_request2;
    ua_cfg.cb.on_call_transfer_status   = &Endpoint::on_call_transfer_status;
    ua_cfg.cb.on_call_replace_request2  = &Endpoint::on_call_replace_request2;
    ua_cfg.cb.on_call_replaced          = &Endpoint::on_call_replaced;
    ua_cfg.cb.on_call_rx_offer          = &Endpoint::on_call_rx_offer;
    ua_cfg.cb.on_call_redirected        = &Endpoint::on_call_redirected;
    ua_cfg.cb.on_call_media_transport_state =
        &Endpoint::on_call_media_transport_state;
    ua_cfg.cb.on_call_media_event       = &Endpoint::on_call_media_event;
    ua_cfg.cb.on_create_media_transport = &Endpoint::on_create_media_transport;

    /* Init! */
    PJSUA2_CHECK_EXPR( pjsua_init(&ua_cfg, &log_cfg, &med_cfg) );
}

void Endpoint::libStart() throw(Error)
{
    PJSUA2_CHECK_EXPR(pjsua_start());
}

void Endpoint::libRegisterWorkerThread(const string &name) throw(Error)
{
    PJSUA2_CHECK_EXPR(pjsua_register_worker_thread(name.c_str()));
}

void Endpoint::libStopWorkerThreads()
{
    pjsua_stop_worker_threads();
}

int Endpoint::libHandleEvents(unsigned msec_timeout)
{
    return pjsua_handle_events(msec_timeout);
}

void Endpoint::libDestroy(unsigned flags) throw(Error)
{
    pj_status_t status;

    status = pjsua_destroy2(flags);

    delete this->writer;
    this->writer = NULL;

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
void Endpoint::natDetectType(void) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_detect_nat_type() );
}

pj_stun_nat_type Endpoint::natGetType() throw(Error)
{
    pj_stun_nat_type type;

    PJSUA2_CHECK_EXPR( pjsua_get_nat_type(&type) );

    return type;
}

void Endpoint::natCheckStunServers(const StringVector &servers,
				   bool wait,
				   Token token) throw(Error)
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
                                         bool notify_cb) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_cancel_stun_resolution(token, notify_cb) );
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

    tcfg = cfg.toPj();
    PJSUA2_CHECK_EXPR( pjsua_transport_create(type,
                                              &tcfg, &tid) );

    return tid;
}

IntVector Endpoint::transportEnum() throw(Error)
{
    pjsua_transport_id tids[32];
    unsigned count = PJ_ARRAY_SIZE(tids);

    PJSUA2_CHECK_EXPR( pjsua_enum_transports(tids, &count) );

    return IntVector(tids, tids+count);
}

TransportInfo Endpoint::transportGetInfo(TransportId id) throw(Error)
{
    pjsua_transport_info pj_tinfo;
    TransportInfo tinfo;

    PJSUA2_CHECK_EXPR( pjsua_transport_get_info(id, &pj_tinfo) );
    tinfo.fromPj(pj_tinfo);

    return tinfo;
}

void Endpoint::transportSetEnable(TransportId id, bool enabled) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_transport_set_enable(id, enabled) );
}

void Endpoint::transportClose(TransportId id) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_transport_close(id, PJ_FALSE) );
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

const AudioMediaVector &Endpoint::mediaEnumPorts() const throw(Error)
{
    return mediaList;
}

void Endpoint::addMedia(AudioMedia &media)
{
    if (mediaExists(media))
	return;

    mediaList.push_back(&media);
}

void Endpoint::removeMedia(AudioMedia &media)
{
    AudioMediaVector::iterator it = std::find(mediaList.begin(),
					      mediaList.end(),
					      &media);

    if (it != mediaList.end())
	mediaList.erase(it);

}

bool Endpoint::mediaExists(const AudioMedia &media) const
{
    AudioMediaVector::const_iterator it = std::find(mediaList.begin(),
						    mediaList.end(),
						    &media);

    return (it != mediaList.end());
}

AudDevManager &Endpoint::audDevManager()
{
    return audioDevMgr;
}
