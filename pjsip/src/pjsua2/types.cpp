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
#include <pjsua2/types.hpp>

using namespace pj;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
inline pj_str_t str2Pj(const string &is)
{
    pj_str_t os;
    os.ptr = (char*)is.c_str();
    os.slen = is.size();
    return os;
}

inline string pj2Str(const pj_str_t &is)
{
    return string(is.ptr, is.slen);
}

///////////////////////////////////////////////////////////////////////////////

Error::Error()
: status(PJ_SUCCESS)
{
}

Error::Error( pj_status_t prm_status,
	      const string &prm_reason,
	      const string &prm_title,
	      const string &prm_src_file,
	      int prm_src_line)
: status(prm_status), reason(prm_reason), title(prm_title),
  srcFile(prm_src_file), srcLine(prm_src_line)
{
    if (this->status != PJ_SUCCESS && prm_reason.empty()) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(this->status, errmsg, sizeof(errmsg));
	this->reason = errmsg;
    }
}

///////////////////////////////////////////////////////////////////////////////

AuthCredInfo::AuthCredInfo()
: dataType(0)
{
}

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

TlsConfig::TlsConfig()
{
    pjsip_tls_setting ts;
    pjsip_tls_setting_default(&ts);
    this->fromPj(ts);
}

pjsip_tls_setting TlsConfig::toPj() const
{
    pjsip_tls_setting ts;

    ts.ca_list_file	= str2Pj(this->CaListFile);
    ts.cert_file	= str2Pj(this->certFile);
    ts.privkey_file	= str2Pj(this->privKeyFile);
    ts.password		= str2Pj(this->password);
    ts.method		= this->method;
    ts.ciphers_num	= this->ciphers.size();
    // The following will only work if sizeof(enum)==sizeof(int)
    pj_assert(sizeof(ts.ciphers[0]) == sizeof(int));
    ts.ciphers		= (pj_ssl_cipher*)&this->ciphers[0];
    ts.verify_server	= this->verifyServer;
    ts.verify_client	= this->verifyClient;
    ts.require_client_cert = this->requireClientCert;
    ts.timeout.sec 	= this->msecTimeout / 1000;
    ts.timeout.msec	= this->msecTimeout % 1000;
    ts.qos_type		= this->qosType;
    ts.qos_params	= this->qosParams;
    ts.qos_ignore_error	= this->qosIgnoreError;

    return ts;
}

void TlsConfig::fromPj(const pjsip_tls_setting &prm)
{
    this->CaListFile 	= pj2Str(prm.ca_list_file);
    this->certFile 	= pj2Str(prm.cert_file);
    this->privKeyFile 	= pj2Str(prm.privkey_file);
    this->password 	= pj2Str(prm.password);
    this->method 	= (pjsip_ssl_method)prm.method;
    // The following will only work if sizeof(enum)==sizeof(int)
    pj_assert(sizeof(prm.ciphers[0]) == sizeof(int));
    this->ciphers 	= IntVector(prm.ciphers, prm.ciphers+prm.ciphers_num);
    this->verifyServer 	= prm.verify_server;
    this->verifyClient 	= prm.verify_client;
    this->requireClientCert = prm.require_client_cert;
    this->msecTimeout 	= PJ_TIME_VAL_MSEC(prm.timeout);
    this->qosType 	= prm.qos_type;
    this->qosParams 	= prm.qos_params;
    this->qosIgnoreError = prm.qos_ignore_error;
}

///////////////////////////////////////////////////////////////////////////////

TransportConfig::TransportConfig()
{
    pjsua_transport_config tc;
    pjsua_transport_config_default(&tc);
    this->fromPj(tc);
}

void TransportConfig::fromPj(const pjsua_transport_config &prm)
{
    this->port 		= prm.port;
    this->publicAddress = pj2Str(prm.public_addr);
    this->boundAddress	= pj2Str(prm.bound_addr);
    this->tlsConfig.fromPj(prm.tls_setting);
    this->qosType	= prm.qos_type;
    this->qosParams	= prm.qos_params;
}

pjsua_transport_config TransportConfig::toPj() const
{
    pjsua_transport_config tc;

    tc.port		= this->port;
    tc.public_addr	= str2Pj(this->publicAddress);
    tc.bound_addr	= str2Pj(this->boundAddress);
    tc.tls_setting	= this->tlsConfig.toPj();
    tc.qos_type		= this->qosType;
    tc.qos_params	= this->qosParams;

    return tc;
}

///////////////////////////////////////////////////////////////////////////////

TransportInfo::TransportInfo(const pjsua_transport_info &info)
{
    this->id = info.id;
    this->type = info.type;
    this->typeName = pj2Str(info.type_name);
    this->info = pj2Str(info.info);
    this->flags = info.flag;

    char straddr[PJ_INET6_ADDRSTRLEN+10];
    pj_sockaddr_print(&info.local_addr, straddr, sizeof(straddr), 3);
    this->localAddress = straddr;

    pj_ansi_snprintf(straddr, sizeof(straddr), "%.*s:%d",
                     (int)info.local_name.host.slen,
                     info.local_name.host.ptr,
                     info.local_name.port);
    this->localName = straddr;
    this->usageCount = info.usage_count;
}

