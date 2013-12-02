/* $Id$ */
/*
 * Copyright (C) 2012-2013 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua2/account.hpp>
#include <pjsua2/call.hpp>
#include <pjsua2/endpoint.hpp>
#include <pj/ctype.h>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE		"call.cpp"

///////////////////////////////////////////////////////////////////////////////

#define SDP_BUFFER_SIZE 1024

MathStat::MathStat()
: n(0), max(0), min(0), last(0), mean(0)
{
}

void MathStat::fromPj(const pj_math_stat &prm)
{
    this->n    = prm.n;
    this->max  = prm.max;
    this->min  = prm.min;
    this->last = prm.last;
    this->mean = prm.mean;
}

void RtcpStreamStat::fromPj(const pjmedia_rtcp_stream_stat &prm)
{
    this->update.fromPj(prm.update);
    this->updateCount     = prm.update_cnt;
    this->pkt             = (unsigned)prm.pkt;
    this->bytes           = (unsigned)prm.bytes;
    this->discard         = prm.discard;
    this->loss            = prm.loss;
    this->reorder         = prm.loss;
    this->dup             = prm.dup;
    this->lossPeriodUsec.fromPj(prm.loss_period);
    this->lossType.burst  = prm.loss_type.burst;
    this->lossType.random = prm.loss_type.random;
    this->jitterUsec.fromPj(prm.jitter);
}

void RtcpSdes::fromPj(const pjmedia_rtcp_sdes &prm)
{
    this->cname = pj2Str(prm.cname);
    this->name  = pj2Str(prm.name);
    this->email = pj2Str(prm.email);
    this->phone = pj2Str(prm.phone);
    this->loc   = pj2Str(prm.loc);
    this->tool  = pj2Str(prm.tool);
    this->note  = pj2Str(prm.note);
}

void RtcpStat::fromPj(const pjmedia_rtcp_stat &prm)
{
    this->start.fromPj(prm.start);
    this->txStat.fromPj(prm.tx);
    this->rxStat.fromPj(prm.rx);
    this->rttUsec.fromPj(prm.rtt);
    this->rtpTxLastTs  = prm.rtp_tx_last_ts;
    this->rtpTxLastSeq = prm.rtp_tx_last_seq;
#if defined(PJMEDIA_RTCP_STAT_HAS_IPDV) && PJMEDIA_RTCP_STAT_HAS_IPDV!=0
    this->rxIpdvUsec.fromPj(prm.rx_ipdv);
#endif
#if defined(PJMEDIA_RTCP_STAT_HAS_RAW_JITTER) && \
    PJMEDIA_RTCP_STAT_HAS_RAW_JITTER!=0
    this->rxRawJitterUsec.fromPj(prm.rx_raw_jitter);
#endif
    this->peerSdes.fromPj(prm.peer_sdes);
}

void JbufState::fromPj(const pjmedia_jb_state &prm)
{
    this->frameSize    = prm.frame_size;
    this->minPrefetch  = prm.min_prefetch;
    this->maxPrefetch  = prm.max_prefetch;
    this->burst        = prm.burst;
    this->prefetch     = prm.prefetch;
    this->size         = prm.size;
    this->avgDelayMsec = prm.avg_delay;
    this->minDelayMsec = prm.min_delay;
    this->maxDelayMsec = prm.max_delay;
    this->devDelayMsec = prm.dev_delay;
    this->avgBurst     = prm.avg_burst;
    this->lost         = prm.lost;
    this->discard      = prm.discard;
    this->empty        = prm.empty;
}

void SdpSession::fromPj(const pjmedia_sdp_session &sdp)
{
    char buf[SDP_BUFFER_SIZE];
    int len;

    len = pjmedia_sdp_print(&sdp, buf, sizeof(buf));
    wholeSdp = (len > -1? string(buf, len): "");
    pjSdpSession = (void *)&sdp;
}

void MediaEvent::fromPj(const pjmedia_event &ev)
{
    type = ev.type;
    if (type == PJMEDIA_EVENT_FMT_CHANGED) {
        data.fmtChanged.newWidth = ev.data.fmt_changed.new_fmt.det.vid.size.w;
        data.fmtChanged.newHeight = ev.data.fmt_changed.new_fmt.det.vid.size.h;
    }
    pjMediaEvent = (void *)&ev;
}

void MediaTransportInfo::fromPj(const pjmedia_transport_info &info)
{
    char straddr[PJ_INET6_ADDRSTRLEN+10];
    
    pj_sockaddr_print(&info.src_rtp_name, straddr, sizeof(straddr), 3);
    srcRtpName = straddr;
    pj_sockaddr_print(&info.src_rtcp_name, straddr, sizeof(straddr), 3);
    srcRtcpName = straddr;
}

//////////////////////////////////////////////////////////////////////////////

/* Call Audio Media. */
class CallAudioMedia : public AudioMedia
{
public:
    /*
     * Set the conference port identification associated with the
     * call audio media.
     */
    void setPortId(int id);
};


void CallAudioMedia::setPortId(int id)
{
    this->id = id;
}

CallOpParam::CallOpParam(bool useDefaultCallSetting)
: statusCode(pjsip_status_code(0)), reason(""), options(0)
{
    if (useDefaultCallSetting)
        opt = CallSetting(true);
}

CallSendRequestParam::CallSendRequestParam()
: method("")
{
}

CallVidSetStreamParam::CallVidSetStreamParam()
{
    pjsua_call_vid_strm_op_param prm;
    
    pjsua_call_vid_strm_op_param_default(&prm);
    this->medIdx = prm.med_idx;
    this->dir    = prm.dir;
    this->capDev = prm.cap_dev;
}

CallSetting::CallSetting(pj_bool_t useDefaultValues)
{
    if (useDefaultValues) {
        pjsua_call_setting setting;
    
        pjsua_call_setting_default(&setting);
        fromPj(setting);
    } else {
        flag                = 0;
        reqKeyframeMethod   = 0;
        audioCount          = 0;
        videoCount          = 0;
    }
}

bool CallSetting::isEmpty() const
{
    return (flag == 0 && reqKeyframeMethod == 0 && audioCount == 0 &&
            videoCount == 0);
}

void CallSetting::fromPj(const pjsua_call_setting &prm)
{
    this->flag              = prm.flag;
    this->reqKeyframeMethod = prm.req_keyframe_method;
    this->audioCount        = prm.aud_cnt;
    this->videoCount        = prm.vid_cnt;
}

pjsua_call_setting CallSetting::toPj() const
{
    pjsua_call_setting setting;

    setting.flag                = this->flag;
    setting.req_keyframe_method = this->reqKeyframeMethod;
    setting.aud_cnt             = this->audioCount;
    setting.vid_cnt             = this->videoCount;
    
    return setting;
}


CallMediaInfo::CallMediaInfo()
{
}

void CallMediaInfo::fromPj(const pjsua_call_media_info &prm)
{
    this->index                     = prm.index;
    this->type                      = prm.type;
    this->dir                       = prm.dir;
    this->status                    = prm.status;
    if (this->type == PJMEDIA_TYPE_AUDIO) {
        this->audioConfSlot         = (int)prm.stream.aud.conf_slot;
    } else if (this->type == PJMEDIA_TYPE_VIDEO) {
        this->videoIncomingWindowId = prm.stream.vid.win_in;
        this->videoCapDev           = prm.stream.vid.cap_dev;
    }
}

void CallInfo::fromPj(const pjsua_call_info &pci)
{
    int mi;
    
    id 			= pci.id;
    role                = pci.role;
    accId               = pci.acc_id;
    localURI            = pj2Str(pci.local_info);
    localContact        = pj2Str(pci.local_contact);
    remoteURI           = pj2Str(pci.remote_info);
    remoteContact       = pj2Str(pci.remote_contact);
    callIdString        = pj2Str(pci.call_id);
    setting.fromPj(pci.setting);
    state               = pci.state;
    stateText           = pj2Str(pci.state_text);
    lastStatusCode      = pci.last_status;
    lastReason          = pj2Str(pci.last_status_text);
    connectDuration.fromPj(pci.connect_duration);
    totalDuration.fromPj(pci.total_duration);
    remOfferer          = pci.rem_offerer;
    remAudioCount       = pci.rem_aud_cnt;
    remVideoCount       = pci.rem_vid_cnt;
    
    for (mi = 0; mi < pci.media_cnt; mi++) {
        CallMediaInfo med;
        
        med.fromPj(pci.media[mi]);
        media.push_back(med);
    }
    for (mi = 0; mi < pci.prov_media_cnt; mi++) {
        CallMediaInfo med;
        
        med.fromPj(pci.prov_media[mi]);
        provMedia.push_back(med);
    }
}

void StreamInfo::fromPj(const pjsua_stream_info &info)
{
    char straddr[PJ_INET6_ADDRSTRLEN+10];

    type = info.type;
    if (type == PJMEDIA_TYPE_AUDIO) {
        proto = info.info.aud.proto;
        dir = info.info.aud.dir;
        pj_sockaddr_print(&info.info.aud.rem_addr, straddr, sizeof(straddr), 3);
        remoteRtpAddress = straddr;
        pj_sockaddr_print(&info.info.aud.rem_rtcp, straddr, sizeof(straddr), 3);
        remoteRtcpAddress = straddr;
        txPt = info.info.aud.tx_pt;
        rxPt = info.info.aud.rx_pt;
        codecName = pj2Str(info.info.aud.fmt.encoding_name);
        codecClockRate = info.info.aud.fmt.clock_rate;
        codecParam = info.info.aud.param;
    } else if (type == PJMEDIA_TYPE_VIDEO) {
        proto = info.info.vid.proto;
        dir = info.info.vid.dir;
        pj_sockaddr_print(&info.info.vid.rem_addr, straddr, sizeof(straddr), 3);
        remoteRtpAddress = straddr;
        pj_sockaddr_print(&info.info.vid.rem_rtcp, straddr, sizeof(straddr), 3);
        remoteRtcpAddress = straddr;
        txPt = info.info.vid.tx_pt;
        rxPt = info.info.vid.rx_pt;
        codecName = pj2Str(info.info.vid.codec_info.encoding_name);
        codecClockRate = info.info.vid.codec_info.clock_rate;
        codecParam = info.info.vid.codec_param;
    }
}

void StreamStat::fromPj(const pjsua_stream_stat &prm)
{
    rtcp.fromPj(prm.rtcp);
    jbuf.fromPj(prm.jbuf);
}

///////////////////////////////////////////////////////////////////////////////

struct call_param
{
    pjsua_msg_data      msg_data;
    pjsua_msg_data     *p_msg_data;
    pjsua_call_setting  opt;
    pjsua_call_setting *p_opt;
    pj_str_t            reason;
    pj_str_t           *p_reason;

public:
    /**
     * Default constructors with specified parameters.
     */
    call_param(const SipTxOption &tx_option);
    call_param(const SipTxOption &tx_option, const CallSetting &setting,
               const string &reason_str);
};

call_param::call_param(const SipTxOption &tx_option)
{
    if (tx_option.isEmpty()) {
        p_msg_data = NULL;
    } else {
        tx_option.toPj(msg_data);
        p_msg_data = &msg_data;
    }
    
    p_opt = NULL;
    p_reason = NULL;
}

call_param::call_param(const SipTxOption &tx_option, const CallSetting &setting,
                       const string &reason_str)
{
    if (tx_option.isEmpty()) {
        p_msg_data = NULL;
    } else {
        tx_option.toPj(msg_data);
        p_msg_data = &msg_data;
    }
    
    if (setting.isEmpty()) {
        p_opt = NULL;
    } else {
        opt = setting.toPj();
        p_opt = &opt;
    }
    
    reason = str2Pj(reason_str);
    p_reason = (reason.slen == 0? NULL: &reason);
}

Call::Call(Account& account, int call_id)
: acc(account), id(call_id)
{
    if (call_id != PJSUA_INVALID_ID)
        pjsua_call_set_user_data(call_id, this);
}

Call::~Call()
{
    /**
     * If this instance is deleted, also hangup the corresponding call in
     * PJSUA library.
     */
    if (id != PJSUA_INVALID_ID && pjsua_get_state() < PJSUA_STATE_CLOSING) {
	pjsua_call_set_user_data(id, NULL);
        if (isActive()) {
            CallOpParam prm;
            hangup(prm);
        }
    }
}

CallInfo Call::getInfo() const throw(Error)
{
    pjsua_call_info pj_ci;
    CallInfo ci;
    
    PJSUA2_CHECK_EXPR( pjsua_call_get_info(id, &pj_ci) );
    ci.fromPj(pj_ci);
    return ci;
}

bool Call::isActive() const
{
    if (id == PJSUA_INVALID_ID)
        return false;
    
    return (pjsua_call_is_active(id) != 0);
}

int Call::getId() const
{
    return id;
}

Call *Call::lookup(int call_id)
{
    Call *call = (Call*)pjsua_call_get_user_data(call_id);
    if (call)
        call->id = call_id;
    return call;
}

bool Call::hasMedia() const
{
    return (pjsua_call_has_media(id) != 0);
}

Media *Call::getMedia(unsigned med_idx) const
{
    /* Check if the media index is valid and if the media has a valid port ID */
    if (med_idx >= medias.size() ||
        (medias[med_idx] && medias[med_idx]->getType() == PJMEDIA_TYPE_AUDIO &&
         ((AudioMedia *)medias[med_idx])->getPortId() == PJSUA_INVALID_ID))
    {
        return NULL;
    }
    
    return medias[med_idx];
}

pjsip_dialog_cap_status Call::remoteHasCap(int htype,
                                           const string &hname,
                                           const string &token) const
{
    pj_str_t pj_hname = str2Pj(hname);
    pj_str_t pj_token = str2Pj(token);
    
    return pjsua_call_remote_has_cap(id, htype,
                                     (htype == PJSIP_H_OTHER)? &pj_hname: NULL,
                                     &pj_token);
}

void Call::setUserData(Token user_data)
{
    userData = user_data;
}

Token Call::getUserData() const
{
    return userData;
}

pj_stun_nat_type Call::getRemNatType() throw(Error)
{
    pj_stun_nat_type nat;
    
    PJSUA2_CHECK_EXPR( pjsua_call_get_rem_nat_type(id, &nat) );
    
    return nat;
}

void Call::makeCall(const string &dst_uri, const CallOpParam &prm) throw(Error)
{
    pj_str_t pj_dst_uri = str2Pj(dst_uri);
    call_param param(prm.txOption, prm.opt, prm.reason);
    
    PJSUA2_CHECK_EXPR( pjsua_call_make_call(acc.getId(), &pj_dst_uri,
                                            param.p_opt, this,
                                            param.p_msg_data, &id) );
}

void Call::answer(const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption, prm.opt, prm.reason);
    
    PJSUA2_CHECK_EXPR( pjsua_call_answer2(id, param.p_opt, prm.statusCode,
                                          param.p_reason, param.p_msg_data) );
}

void Call::hangup(const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption, prm.opt, prm.reason);
    
    PJSUA2_CHECK_EXPR( pjsua_call_hangup(id, prm.statusCode, param.p_reason,
                                         param.p_msg_data) );
}

void Call::setHold(const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption, prm.opt, prm.reason);
    
    PJSUA2_CHECK_EXPR( pjsua_call_set_hold2(id, prm.options, param.p_msg_data));
}

void Call::reinvite(const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption, prm.opt, prm.reason);

    PJSUA2_CHECK_EXPR( pjsua_call_reinvite2(id, param.p_opt, param.p_msg_data));
}

void Call::update(const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption, prm.opt, prm.reason);
    
    PJSUA2_CHECK_EXPR( pjsua_call_update2(id, param.p_opt, param.p_msg_data) );
}

void Call::xfer(const string &dest, const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption);
    pj_str_t pj_dest = str2Pj(dest);
    
    PJSUA2_CHECK_EXPR( pjsua_call_xfer(id, &pj_dest, param.p_msg_data) );
}

void Call::xferReplaces(const Call& dest_call,
                  const CallOpParam &prm) throw(Error)
{
    call_param param(prm.txOption);
    
    PJSUA2_CHECK_EXPR(pjsua_call_xfer_replaces(id, dest_call.getId(),
                                               prm.options, param.p_msg_data) );
}

void Call::processRedirect(pjsip_redirect_op cmd) throw(Error)
{
    PJSUA2_CHECK_EXPR(pjsua_call_process_redirect(id, cmd));
}

void Call::dialDtmf(const string &digits) throw(Error)
{
    pj_str_t pj_digits = str2Pj(digits);
    
    PJSUA2_CHECK_EXPR(pjsua_call_dial_dtmf(id, &pj_digits));
}

void Call::sendInstantMessage(const SendInstantMessageParam& prm)
    throw(Error)
{
    pj_str_t mime_type = str2Pj(prm.contentType);
    pj_str_t content = str2Pj(prm.content);
    call_param param(prm.txOption);

    PJSUA2_CHECK_EXPR(pjsua_call_send_im(id, &mime_type, &content,
                                         param.p_msg_data, prm.userData) );
}

void Call::sendTypingIndication(const SendTypingIndicationParam &prm)
    throw(Error)
{
    call_param param(prm.txOption);
    
    PJSUA2_CHECK_EXPR(pjsua_call_send_typing_ind(id,
                                                 (prm.isTyping?
                                                  PJ_TRUE: PJ_FALSE),
                                                 param.p_msg_data) );
}

void Call::sendRequest(const CallSendRequestParam &prm) throw(Error)
{
    pj_str_t method = str2Pj(prm.method);
    call_param param(prm.txOption);
    
    PJSUA2_CHECK_EXPR(pjsua_call_send_request(id, &method, param.p_msg_data) );
}

string Call::dump(bool with_media, const string indent) throw(Error)
{
#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    char buffer[1024 * 10];
#else
    char buffer[1024 * 3];
#endif

    PJSUA2_CHECK_EXPR(pjsua_call_dump(id, (with_media? PJ_TRUE: PJ_FALSE),
                                      buffer, sizeof(buffer),
                                      indent.c_str()));
    
    return buffer;
}

int Call::vidGetStreamIdx() const
{
#if PJSUA_HAS_VIDEO
    return pjsua_call_get_vid_stream_idx(id);
#else
    return PJSUA_INVALID_ID;
#endif
}

bool Call::vidStreamIsRunning(int med_idx, pjmedia_dir dir) const
{
#if PJSUA_HAS_VIDEO
    return pjsua_call_vid_stream_is_running(id, med_idx, dir);
#else
    return false;
#endif
}

void Call::vidSetStream(pjsua_call_vid_strm_op op,
                        const CallVidSetStreamParam &param) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjsua_call_vid_strm_op_param prm;
    
    prm.med_idx = param.medIdx;
    prm.dir = param.dir;
    prm.cap_dev = param.capDev;
    PJSUA2_CHECK_EXPR( pjsua_call_set_vid_strm(id, op, &prm) );
#else
    PJSUA2_RAISE_ERROR(PJ_EINVOP);
#endif
}

StreamInfo Call::getStreamInfo(unsigned med_idx) const throw(Error)
{
    pjsua_stream_info pj_si;
    StreamInfo si;
    
    PJSUA2_CHECK_EXPR( pjsua_call_get_stream_info(id, med_idx, &pj_si) );
    si.fromPj(pj_si);
    return si;
}

StreamStat Call::getStreamStat(unsigned med_idx) const throw(Error)
{
    pjsua_stream_stat pj_ss;
    StreamStat ss;
    
    PJSUA2_CHECK_EXPR( pjsua_call_get_stream_stat(id, med_idx, &pj_ss) );
    ss.fromPj(pj_ss);
    return ss;
}

MediaTransportInfo Call::getMedTransportInfo(unsigned med_idx) const
    throw(Error)
{
    pjmedia_transport_info pj_mti;
    MediaTransportInfo mti;
    
    PJSUA2_CHECK_EXPR( pjsua_call_get_med_transport_info(id, med_idx,
                                                         &pj_mti) );
    mti.fromPj(pj_mti);
    return mti;
}

void Call::processMediaUpdate(OnCallMediaStateParam &prm)
{
    pjsua_call_info pj_ci;
    unsigned mi;
    
    if (pjsua_call_get_info(id, &pj_ci) == PJ_SUCCESS) {
        for (mi = 0; mi < pj_ci.media_cnt; mi++) {
            if (mi >= medias.size()) {
                if (pj_ci.media[mi].type == PJMEDIA_TYPE_AUDIO) {
                    medias.push_back(new CallAudioMedia);
                } else {
                    medias.push_back(NULL);
                }
            }
            
            if (pj_ci.media[mi].type == PJMEDIA_TYPE_AUDIO) {
                CallAudioMedia *aud_med = (CallAudioMedia *)medias[mi];
                
                aud_med->setPortId(pj_ci.media[mi].stream.aud.conf_slot);
                /* Add media if the conference slot ID is valid. */
                if (pj_ci.media[mi].stream.aud.conf_slot != PJSUA_INVALID_ID)
                {
                    Endpoint::instance().addMedia((AudioMedia &)*aud_med);
                } else {
                    Endpoint::instance().removeMedia((AudioMedia &)*aud_med);
                }
            }
        }
    }
    
    /* Call media state callback. */
    onCallMediaState(prm);
}

void Call::processStateChange(OnCallStateParam &prm)
{
    pjsua_call_info pj_ci;
    unsigned mi;
    
    if (pjsua_call_get_info(id, &pj_ci) == PJ_SUCCESS &&
        pj_ci.state == PJSIP_INV_STATE_DISCONNECTED)
    {
        /* Clear medias. */
        for (mi = 0; mi < medias.size(); mi++) {
            if (medias[mi])
                delete medias[mi];
        }
        medias.clear();
    }
    
    onCallState(prm);
    /* If the state is DISCONNECTED, this call may have already been deleted
     * by the application in the callback, so do not access it anymore here.
     */
}
