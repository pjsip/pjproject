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
#include <pjsua2/account.hpp>
#include <pjsua2/endpoint.hpp>
#include <pj/ctype.h>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE		"account.cpp"

///////////////////////////////////////////////////////////////////////////////

void AccountRegConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountRegConfig");

    NODE_READ_STRING	(this_node, registrarUri);
    NODE_READ_BOOL	(this_node, registerOnAdd);
    NODE_READ_UNSIGNED	(this_node, timeoutSec);
    NODE_READ_UNSIGNED	(this_node, retryIntervalSec);
    NODE_READ_UNSIGNED	(this_node, firstRetryIntervalSec);
    NODE_READ_UNSIGNED	(this_node, delayBeforeRefreshSec);
    NODE_READ_BOOL	(this_node, dropCallsOnFail);
    NODE_READ_UNSIGNED	(this_node, unregWaitSec);
    NODE_READ_UNSIGNED	(this_node, proxyUse);

    readSipHeaders(this_node, "headers", headers);
}

void AccountRegConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountRegConfig");

    NODE_WRITE_STRING	(this_node, registrarUri);
    NODE_WRITE_BOOL	(this_node, registerOnAdd);
    NODE_WRITE_UNSIGNED	(this_node, timeoutSec);
    NODE_WRITE_UNSIGNED	(this_node, retryIntervalSec);
    NODE_WRITE_UNSIGNED	(this_node, firstRetryIntervalSec);
    NODE_WRITE_UNSIGNED	(this_node, delayBeforeRefreshSec);
    NODE_WRITE_BOOL	(this_node, dropCallsOnFail);
    NODE_WRITE_UNSIGNED	(this_node, unregWaitSec);
    NODE_WRITE_UNSIGNED	(this_node, proxyUse);

    writeSipHeaders(this_node, "headers", headers);
}

///////////////////////////////////////////////////////////////////////////////

void AccountSipConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountSipConfig");

    NODE_READ_STRINGV	(this_node, proxies);
    NODE_READ_STRING	(this_node, contactForced);
    NODE_READ_STRING	(this_node, contactParams);
    NODE_READ_STRING	(this_node, contactUriParams);
    NODE_READ_BOOL	(this_node, authInitialEmpty);
    NODE_READ_STRING	(this_node, authInitialAlgorithm);
    NODE_READ_INT	(this_node, transportId);

    ContainerNode creds_node = this_node.readArray("authCreds");
    authCreds.resize(0);
    while (creds_node.hasUnread()) {
	AuthCredInfo cred;
	cred.readObject(creds_node);
	authCreds.push_back(cred);
    }
}

void AccountSipConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountSipConfig");

    NODE_WRITE_STRINGV	(this_node, proxies);
    NODE_WRITE_STRING	(this_node, contactForced);
    NODE_WRITE_STRING	(this_node, contactParams);
    NODE_WRITE_STRING	(this_node, contactUriParams);
    NODE_WRITE_BOOL	(this_node, authInitialEmpty);
    NODE_WRITE_STRING	(this_node, authInitialAlgorithm);
    NODE_WRITE_INT	(this_node, transportId);

    ContainerNode creds_node = this_node.writeNewArray("authCreds");
    for (unsigned i=0; i<authCreds.size(); ++i) {
	authCreds[i].writeObject(creds_node);
    }
}

///////////////////////////////////////////////////////////////////////////////

void AccountCallConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountCallConfig");

    NODE_READ_NUM_T   ( this_node, pjsua_call_hold_type, holdType);
    NODE_READ_NUM_T   ( this_node, pjsua_100rel_use, prackUse);
    NODE_READ_NUM_T   ( this_node, pjsua_sip_timer_use, timerUse);
    NODE_READ_UNSIGNED( this_node, timerMinSESec);
    NODE_READ_UNSIGNED( this_node, timerSessExpiresSec);
}

void AccountCallConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountCallConfig");

    NODE_WRITE_NUM_T   ( this_node, pjsua_call_hold_type, holdType);
    NODE_WRITE_NUM_T   ( this_node, pjsua_100rel_use, prackUse);
    NODE_WRITE_NUM_T   ( this_node, pjsua_sip_timer_use, timerUse);
    NODE_WRITE_UNSIGNED( this_node, timerMinSESec);
    NODE_WRITE_UNSIGNED( this_node, timerSessExpiresSec);
}

///////////////////////////////////////////////////////////////////////////////

void AccountPresConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountPresConfig");

    NODE_READ_BOOL    ( this_node, publishEnabled);
    NODE_READ_BOOL    ( this_node, publishQueue);
    NODE_READ_UNSIGNED( this_node, publishShutdownWaitMsec);
    NODE_READ_STRING  ( this_node, pidfTupleId);

    readSipHeaders(this_node, "headers", headers);
}

void AccountPresConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountPresConfig");

    NODE_WRITE_BOOL    ( this_node, publishEnabled);
    NODE_WRITE_BOOL    ( this_node, publishQueue);
    NODE_WRITE_UNSIGNED( this_node, publishShutdownWaitMsec);
    NODE_WRITE_STRING  ( this_node, pidfTupleId);

    writeSipHeaders(this_node, "headers", headers);
}

///////////////////////////////////////////////////////////////////////////////

void AccountMwiConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountMwiConfig");

    NODE_READ_BOOL    ( this_node, enabled);
    NODE_READ_UNSIGNED( this_node, expirationSec);
}

void AccountMwiConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountMwiConfig");

    NODE_WRITE_BOOL    ( this_node, enabled);
    NODE_WRITE_UNSIGNED( this_node, expirationSec);
}

///////////////////////////////////////////////////////////////////////////////

void AccountNatConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountNatConfig");

    NODE_READ_NUM_T   ( this_node, pjsua_stun_use, sipStunUse);
    NODE_READ_NUM_T   ( this_node, pjsua_stun_use, mediaStunUse);
    NODE_READ_BOOL    ( this_node, iceEnabled);
    NODE_READ_INT     ( this_node, iceMaxHostCands);
    NODE_READ_BOOL    ( this_node, iceAggressiveNomination);
    NODE_READ_UNSIGNED( this_node, iceNominatedCheckDelayMsec);
    NODE_READ_INT     ( this_node, iceWaitNominationTimeoutMsec);
    NODE_READ_BOOL    ( this_node, iceNoRtcp);
    NODE_READ_BOOL    ( this_node, iceAlwaysUpdate);
    NODE_READ_BOOL    ( this_node, turnEnabled);
    NODE_READ_STRING  ( this_node, turnServer);
    NODE_READ_NUM_T   ( this_node, pj_turn_tp_type, turnConnType);
    NODE_READ_STRING  ( this_node, turnUserName);
    NODE_READ_INT     ( this_node, turnPasswordType);
    NODE_READ_STRING  ( this_node, turnPassword);
    NODE_READ_INT     ( this_node, contactRewriteUse);
    NODE_READ_INT     ( this_node, contactRewriteMethod);
    NODE_READ_INT     ( this_node, viaRewriteUse);
    NODE_READ_INT     ( this_node, sdpNatRewriteUse);
    NODE_READ_INT     ( this_node, sipOutboundUse);
    NODE_READ_STRING  ( this_node, sipOutboundInstanceId);
    NODE_READ_STRING  ( this_node, sipOutboundRegId);
    NODE_READ_UNSIGNED( this_node, udpKaIntervalSec);
    NODE_READ_STRING  ( this_node, udpKaData);
}

void AccountNatConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountNatConfig");

    NODE_WRITE_NUM_T   ( this_node, pjsua_stun_use, sipStunUse);
    NODE_WRITE_NUM_T   ( this_node, pjsua_stun_use, mediaStunUse);
    NODE_WRITE_BOOL    ( this_node, iceEnabled);
    NODE_WRITE_INT     ( this_node, iceMaxHostCands);
    NODE_WRITE_BOOL    ( this_node, iceAggressiveNomination);
    NODE_WRITE_UNSIGNED( this_node, iceNominatedCheckDelayMsec);
    NODE_WRITE_INT     ( this_node, iceWaitNominationTimeoutMsec);
    NODE_WRITE_BOOL    ( this_node, iceNoRtcp);
    NODE_WRITE_BOOL    ( this_node, iceAlwaysUpdate);
    NODE_WRITE_BOOL    ( this_node, turnEnabled);
    NODE_WRITE_STRING  ( this_node, turnServer);
    NODE_WRITE_NUM_T   ( this_node, pj_turn_tp_type, turnConnType);
    NODE_WRITE_STRING  ( this_node, turnUserName);
    NODE_WRITE_INT     ( this_node, turnPasswordType);
    NODE_WRITE_STRING  ( this_node, turnPassword);
    NODE_WRITE_INT     ( this_node, contactRewriteUse);
    NODE_WRITE_INT     ( this_node, contactRewriteMethod);
    NODE_WRITE_INT     ( this_node, viaRewriteUse);
    NODE_WRITE_INT     ( this_node, sdpNatRewriteUse);
    NODE_WRITE_INT     ( this_node, sipOutboundUse);
    NODE_WRITE_STRING  ( this_node, sipOutboundInstanceId);
    NODE_WRITE_STRING  ( this_node, sipOutboundRegId);
    NODE_WRITE_UNSIGNED( this_node, udpKaIntervalSec);
    NODE_WRITE_STRING  ( this_node, udpKaData);
}

///////////////////////////////////////////////////////////////////////////////

void AccountMediaConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountMediaConfig");

    NODE_READ_BOOL    ( this_node, lockCodecEnabled);
    NODE_READ_BOOL    ( this_node, streamKaEnabled);
    NODE_READ_NUM_T   ( this_node, pjmedia_srtp_use, srtpUse);
    NODE_READ_INT     ( this_node, srtpSecureSignaling);
    NODE_READ_NUM_T   ( this_node, pjsua_ipv6_use, ipv6Use);
    NODE_READ_OBJ     ( this_node, transportConfig);
}

void AccountMediaConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountMediaConfig");

    NODE_WRITE_BOOL    ( this_node, lockCodecEnabled);
    NODE_WRITE_BOOL    ( this_node, streamKaEnabled);
    NODE_WRITE_NUM_T   ( this_node, pjmedia_srtp_use, srtpUse);
    NODE_WRITE_INT     ( this_node, srtpSecureSignaling);
    NODE_WRITE_NUM_T   ( this_node, pjsua_ipv6_use, ipv6Use);
    NODE_WRITE_OBJ     ( this_node, transportConfig);
}

///////////////////////////////////////////////////////////////////////////////

void AccountVideoConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountVideoConfig");

    NODE_READ_BOOL    ( this_node, autoShowIncoming);
    NODE_READ_BOOL    ( this_node, autoTransmitOutgoing);
    NODE_READ_UNSIGNED( this_node, windowFlags);
    NODE_READ_NUM_T   ( this_node, pjmedia_vid_dev_index, defaultCaptureDevice);
    NODE_READ_NUM_T   ( this_node, pjmedia_vid_dev_index, defaultRenderDevice);
    NODE_READ_NUM_T   ( this_node, pjmedia_vid_stream_rc_method, rateControlMethod);
    NODE_READ_UNSIGNED( this_node, rateControlBandwidth);
}

void AccountVideoConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountVideoConfig");

    NODE_WRITE_BOOL    ( this_node, autoShowIncoming);
    NODE_WRITE_BOOL    ( this_node, autoTransmitOutgoing);
    NODE_WRITE_UNSIGNED( this_node, windowFlags);
    NODE_WRITE_NUM_T   ( this_node, pjmedia_vid_dev_index, defaultCaptureDevice);
    NODE_WRITE_NUM_T   ( this_node, pjmedia_vid_dev_index, defaultRenderDevice);
    NODE_WRITE_NUM_T   ( this_node, pjmedia_vid_stream_rc_method, rateControlMethod);
    NODE_WRITE_UNSIGNED( this_node, rateControlBandwidth);
}

///////////////////////////////////////////////////////////////////////////////

AccountConfig::AccountConfig()
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_config_default(&acc_cfg);
    pjsua_media_config med_cfg;
    pjsua_media_config_default(&med_cfg);
    fromPj(acc_cfg, &med_cfg);
}

/* Convert to pjsip. */
pjsua_acc_config AccountConfig::toPj() const
{
    pjsua_acc_config ret;
    unsigned i;

    pjsua_acc_config_default(&ret);

    // Global
    ret.priority		= priority;
    ret.id			= str2Pj(idUri);

    // AccountRegConfig
    ret.reg_uri			= str2Pj(regConfig.registrarUri);
    ret.register_on_acc_add	= regConfig.registerOnAdd;
    ret.reg_timeout		= regConfig.timeoutSec;
    ret.reg_retry_interval	= regConfig.retryIntervalSec;
    ret.reg_first_retry_interval= regConfig.firstRetryIntervalSec;
    ret.reg_delay_before_refresh= regConfig.delayBeforeRefreshSec;
    ret.drop_calls_on_reg_fail	= regConfig.dropCallsOnFail;
    ret.unreg_timeout		= regConfig.unregWaitSec;
    ret.reg_use_proxy		= regConfig.proxyUse;
    for (i=0; i<regConfig.headers.size(); ++i) {
	pj_list_push_back(&ret.reg_hdr_list, &regConfig.headers[i].toPj());
    }

    // AccountSipConfig
    ret.cred_count = 0;
    if (sipConfig.authCreds.size() > PJ_ARRAY_SIZE(ret.cred_info))
	PJSUA2_RAISE_ERROR(PJ_ETOOMANY);
    for (i=0; i<sipConfig.authCreds.size(); ++i) {
	const AuthCredInfo &src = sipConfig.authCreds[i];
	pjsip_cred_info *dst = &ret.cred_info[i];

	dst->realm	= str2Pj(src.realm);
	dst->scheme	= str2Pj(src.scheme);
	dst->username	= str2Pj(src.username);
	dst->data_type	= src.dataType;
	dst->data	= str2Pj(src.data);
	dst->ext.aka.k	= str2Pj(src.akaK);
	dst->ext.aka.op	= str2Pj(src.akaOp);
	dst->ext.aka.amf= str2Pj(src.akaAmf);

	ret.cred_count++;
    }
    ret.proxy_cnt = 0;
    if (sipConfig.proxies.size() > PJ_ARRAY_SIZE(ret.proxy))
	PJSUA2_RAISE_ERROR(PJ_ETOOMANY);
    for (i=0; i<sipConfig.proxies.size(); ++i) {
	ret.proxy[ret.proxy_cnt++] = str2Pj(sipConfig.proxies[i]);
    }
    ret.force_contact		= str2Pj(sipConfig.contactForced);
    ret.contact_params		= str2Pj(sipConfig.contactParams);
    ret.contact_uri_params	= str2Pj(sipConfig.contactUriParams);
    ret.auth_pref.initial_auth	= sipConfig.authInitialEmpty;
    ret.auth_pref.algorithm	= str2Pj(sipConfig.authInitialAlgorithm);
    ret.transport_id		= sipConfig.transportId;

    // AccountCallConfig
    ret.call_hold_type		= callConfig.holdType;
    ret.require_100rel		= callConfig.prackUse;
    ret.use_timer		= callConfig.timerUse;
    ret.timer_setting.min_se	= callConfig.timerMinSESec;
    ret.timer_setting.sess_expires = callConfig.timerSessExpiresSec;

    // AccountPresConfig
    for (i=0; i<presConfig.headers.size(); ++i) {
	pj_list_push_back(&ret.sub_hdr_list, &presConfig.headers[i].toPj());
    }
    ret.publish_enabled		= presConfig.publishEnabled;
    ret.publish_opt.queue_request= presConfig.publishQueue;
    ret.unpublish_max_wait_time_msec = presConfig.publishShutdownWaitMsec;
    ret.pidf_tuple_id		= str2Pj(presConfig.pidfTupleId);

    // AccountNatConfig
    ret.sip_stun_use		= natConfig.sipStunUse;
    ret.media_stun_use		= natConfig.mediaStunUse;
    ret.ice_cfg_use		= PJSUA_ICE_CONFIG_USE_CUSTOM;
    ret.ice_cfg.enable_ice	= natConfig.iceEnabled;
    ret.ice_cfg.ice_max_host_cands = natConfig.iceMaxHostCands;
    ret.ice_cfg.ice_opt.aggressive = natConfig.iceAggressiveNomination;
    ret.ice_cfg.ice_opt.nominated_check_delay = natConfig.iceNominatedCheckDelayMsec;
    ret.ice_cfg.ice_opt.controlled_agent_want_nom_timeout = natConfig.iceWaitNominationTimeoutMsec;
    ret.ice_cfg.ice_no_rtcp	= natConfig.iceNoRtcp;
    ret.ice_cfg.ice_always_update = natConfig.iceAlwaysUpdate;

    ret.turn_cfg_use 		= PJSUA_TURN_CONFIG_USE_CUSTOM;
    ret.turn_cfg.enable_turn	= natConfig.turnEnabled;
    ret.turn_cfg.turn_server	= str2Pj(natConfig.turnServer);
    ret.turn_cfg.turn_conn_type	= natConfig.turnConnType;
    ret.turn_cfg.turn_auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
    ret.turn_cfg.turn_auth_cred.data.static_cred.username = str2Pj(natConfig.turnUserName);
    ret.turn_cfg.turn_auth_cred.data.static_cred.data_type = (pj_stun_passwd_type)natConfig.turnPasswordType;
    ret.turn_cfg.turn_auth_cred.data.static_cred.data = str2Pj(natConfig.turnPassword);
    ret.turn_cfg.turn_auth_cred.data.static_cred.realm = pj_str((char*)"");
    ret.turn_cfg.turn_auth_cred.data.static_cred.nonce = pj_str((char*)"");

    ret.allow_contact_rewrite	= natConfig.contactRewriteUse;
    ret.contact_rewrite_method	= natConfig.contactRewriteMethod;
    ret.allow_via_rewrite	= natConfig.viaRewriteUse;
    ret.allow_sdp_nat_rewrite	= natConfig.sdpNatRewriteUse;
    ret.use_rfc5626		= natConfig.sipOutboundUse;
    ret.rfc5626_instance_id	= str2Pj(natConfig.sipOutboundInstanceId);
    ret.rfc5626_reg_id		= str2Pj(natConfig.sipOutboundRegId);
    ret.ka_interval		= natConfig.udpKaIntervalSec;
    ret.ka_data			= str2Pj(natConfig.udpKaData);

    // AccountMediaConfig
    ret.rtp_cfg			= mediaConfig.transportConfig.toPj();
    ret.lock_codec		= mediaConfig.lockCodecEnabled;
#if defined(PJMEDIA_STREAM_ENABLE_KA) && (PJMEDIA_STREAM_ENABLE_KA != 0)
    ret.use_stream_ka		= mediaConfig.streamKaEnabled;
#endif
    ret.use_srtp		= mediaConfig.srtpUse;
    ret.srtp_secure_signaling	= mediaConfig.srtpSecureSignaling;
    ret.ipv6_media_use		= mediaConfig.ipv6Use;

    // AccountVideoConfig
    ret.vid_in_auto_show	= videoConfig.autoShowIncoming;
    ret.vid_out_auto_transmit	= videoConfig.autoTransmitOutgoing;
    ret.vid_wnd_flags		= videoConfig.windowFlags;
    ret.vid_cap_dev		= videoConfig.defaultCaptureDevice;
    ret.vid_rend_dev		= videoConfig.defaultRenderDevice;
    ret.vid_stream_rc_cfg.method= videoConfig.rateControlMethod;
    ret.vid_stream_rc_cfg.bandwidth = videoConfig.rateControlBandwidth;

    return ret;
}

/* Initialize from pjsip. */
void AccountConfig::fromPj(const pjsua_acc_config &prm,
                           const pjsua_media_config *mcfg)
{
    const pjsip_hdr *hdr;
    unsigned i;

    // Global
    priority			= prm.priority;
    idUri			= pj2Str(prm.id);

    // AccountRegConfig
    regConfig.registrarUri	= pj2Str(prm.reg_uri);
    regConfig.registerOnAdd	= (prm.register_on_acc_add != 0);
    regConfig.timeoutSec	= prm.reg_timeout;
    regConfig.retryIntervalSec	= prm.reg_retry_interval;
    regConfig.firstRetryIntervalSec = prm.reg_first_retry_interval;
    regConfig.delayBeforeRefreshSec = prm.reg_delay_before_refresh;
    regConfig.dropCallsOnFail	= prm.drop_calls_on_reg_fail;
    regConfig.unregWaitSec	= prm.unreg_timeout;
    regConfig.proxyUse		= prm.reg_use_proxy;
    regConfig.headers.clear();
    hdr = prm.reg_hdr_list.next;
    while (hdr != &prm.reg_hdr_list) {
	SipHeader new_hdr;
	new_hdr.fromPj(hdr);

	regConfig.headers.push_back(new_hdr);

	hdr = hdr->next;
    }

    // AccountSipConfig
    sipConfig.authCreds.clear();
    for (i=0; i<prm.cred_count; ++i) {
	AuthCredInfo cred;
	const pjsip_cred_info &src = prm.cred_info[i];

	cred.realm	= pj2Str(src.realm);
	cred.scheme	= pj2Str(src.scheme);
	cred.username	= pj2Str(src.username);
	cred.dataType	= src.data_type;
	cred.data	= pj2Str(src.data);
	cred.akaK	= pj2Str(src.ext.aka.k);
	cred.akaOp	= pj2Str(src.ext.aka.op);
	cred.akaAmf	= pj2Str(src.ext.aka.amf);

	sipConfig.authCreds.push_back(cred);
    }
    sipConfig.proxies.clear();
    for (i=0; i<prm.proxy_cnt; ++i) {
	sipConfig.proxies.push_back(pj2Str(prm.proxy[i]));
    }
    sipConfig.contactForced	= pj2Str(prm.force_contact);
    sipConfig.contactParams	= pj2Str(prm.contact_params);
    sipConfig.contactUriParams	= pj2Str(prm.contact_uri_params);
    sipConfig.authInitialEmpty	= prm.auth_pref.initial_auth;
    sipConfig.authInitialAlgorithm = pj2Str(prm.auth_pref.algorithm);
    sipConfig.transportId	= prm.transport_id;

    // AccountCallConfig
    callConfig.holdType		= prm.call_hold_type;
    callConfig.prackUse		= prm.require_100rel;
    callConfig.timerUse		= prm.use_timer;
    callConfig.timerMinSESec	= prm.timer_setting.min_se;
    callConfig.timerSessExpiresSec = prm.timer_setting.sess_expires;

    // AccountPresConfig
    presConfig.headers.clear();
    hdr = prm.sub_hdr_list.next;
    while (hdr != &prm.sub_hdr_list) {
	SipHeader new_hdr;
	new_hdr.fromPj(hdr);
	presConfig.headers.push_back(new_hdr);
	hdr = hdr->next;
    }
    presConfig.publishEnabled	= prm.publish_enabled;
    presConfig.publishQueue	= prm.publish_opt.queue_request;
    presConfig.publishShutdownWaitMsec = prm.unpublish_max_wait_time_msec;
    presConfig.pidfTupleId	= pj2Str(prm.pidf_tuple_id);

    // AccountMwiConfig
    mwiConfig.enabled		= prm.mwi_enabled;
    mwiConfig.expirationSec	= prm.mwi_expires;

    // AccountNatConfig
    natConfig.sipStunUse	= prm.sip_stun_use;
    natConfig.mediaStunUse	= prm.media_stun_use;
    if (prm.ice_cfg_use == PJSUA_ICE_CONFIG_USE_CUSTOM) {
	natConfig.iceEnabled = prm.ice_cfg.enable_ice;
	natConfig.iceMaxHostCands = prm.ice_cfg.ice_max_host_cands;
	natConfig.iceAggressiveNomination = prm.ice_cfg.ice_opt.aggressive;
	natConfig.iceNominatedCheckDelayMsec = prm.ice_cfg.ice_opt.nominated_check_delay;
	natConfig.iceWaitNominationTimeoutMsec = prm.ice_cfg.ice_opt.controlled_agent_want_nom_timeout;
	natConfig.iceNoRtcp	= prm.ice_cfg.ice_no_rtcp;
	natConfig.iceAlwaysUpdate = prm.ice_cfg.ice_always_update;
    } else {
	pjsua_media_config default_mcfg;
	if (!mcfg) {
	    pjsua_media_config_default(&default_mcfg);
	    mcfg = &default_mcfg;
	}
	natConfig.iceEnabled	= mcfg->enable_ice;
	natConfig.iceMaxHostCands= mcfg->ice_max_host_cands;
	natConfig.iceAggressiveNomination = mcfg->ice_opt.aggressive;
	natConfig.iceNominatedCheckDelayMsec = mcfg->ice_opt.nominated_check_delay;
	natConfig.iceWaitNominationTimeoutMsec = mcfg->ice_opt.controlled_agent_want_nom_timeout;
	natConfig.iceNoRtcp	= mcfg->ice_no_rtcp;
	natConfig.iceAlwaysUpdate = mcfg->ice_always_update;
    }

    if (prm.turn_cfg_use == PJSUA_TURN_CONFIG_USE_CUSTOM) {
	natConfig.turnEnabled	= prm.turn_cfg.enable_turn;
	natConfig.turnServer	= pj2Str(prm.turn_cfg.turn_server);
	natConfig.turnConnType	= prm.turn_cfg.turn_conn_type;
	natConfig.turnUserName	= pj2Str(prm.turn_cfg.turn_auth_cred.data.static_cred.username);
	natConfig.turnPasswordType = prm.turn_cfg.turn_auth_cred.data.static_cred.data_type;
	natConfig.turnPassword	= pj2Str(prm.turn_cfg.turn_auth_cred.data.static_cred.data);
    } else {
	pjsua_media_config default_mcfg;
	if (!mcfg) {
	    pjsua_media_config_default(&default_mcfg);
	    mcfg = &default_mcfg;
	}
	natConfig.turnEnabled	= mcfg->enable_turn;
	natConfig.turnServer	= pj2Str(mcfg->turn_server);
	natConfig.turnConnType	= mcfg->turn_conn_type;
	natConfig.turnUserName	= pj2Str(mcfg->turn_auth_cred.data.static_cred.username);
	natConfig.turnPasswordType = mcfg->turn_auth_cred.data.static_cred.data_type;
	natConfig.turnPassword	= pj2Str(mcfg->turn_auth_cred.data.static_cred.data);
    }
    natConfig.contactRewriteUse	= prm.allow_contact_rewrite;
    natConfig.contactRewriteMethod = prm.contact_rewrite_method;
    natConfig.viaRewriteUse	= prm.allow_via_rewrite;
    natConfig.sdpNatRewriteUse	= prm.allow_sdp_nat_rewrite;
    natConfig.sipOutboundUse	= prm.use_rfc5626;
    natConfig.sipOutboundInstanceId = pj2Str(prm.rfc5626_instance_id);
    natConfig.sipOutboundRegId	= pj2Str(prm.rfc5626_reg_id);
    natConfig.udpKaIntervalSec	= prm.ka_interval;
    natConfig.udpKaData		= pj2Str(prm.ka_data);

    // AccountMediaConfig
    mediaConfig.transportConfig.fromPj(prm.rtp_cfg);
    mediaConfig.lockCodecEnabled= prm.lock_codec;
#if defined(PJMEDIA_STREAM_ENABLE_KA) && (PJMEDIA_STREAM_ENABLE_KA != 0)
    mediaConfig.streamKaEnabled	= prm.use_stream_ka;
#else
    mediaConfig.streamKaEnabled	= false;
#endif
    mediaConfig.srtpUse		= prm.use_srtp;
    mediaConfig.srtpSecureSignaling = prm.srtp_secure_signaling;
    mediaConfig.ipv6Use		= prm.ipv6_media_use;

    // AccountVideoConfig
    videoConfig.autoShowIncoming 	= prm.vid_in_auto_show;
    videoConfig.autoTransmitOutgoing	= prm.vid_out_auto_transmit;
    videoConfig.windowFlags		= prm.vid_wnd_flags;
    videoConfig.defaultCaptureDevice	= prm.vid_cap_dev;
    videoConfig.defaultRenderDevice	= prm.vid_rend_dev;
    videoConfig.rateControlMethod	= prm.vid_stream_rc_cfg.method;
    videoConfig.rateControlBandwidth	= prm.vid_stream_rc_cfg.bandwidth;
}

void AccountConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("AccountConfig");

    NODE_READ_INT     ( this_node, priority);
    NODE_READ_STRING  ( this_node, idUri);
    NODE_READ_OBJ     ( this_node, regConfig);
    NODE_READ_OBJ     ( this_node, sipConfig);
    NODE_READ_OBJ     ( this_node, callConfig);
    NODE_READ_OBJ     ( this_node, presConfig);
    NODE_READ_OBJ     ( this_node, mwiConfig);
    NODE_READ_OBJ     ( this_node, natConfig);
    NODE_READ_OBJ     ( this_node, mediaConfig);
    NODE_READ_OBJ     ( this_node, videoConfig);
}

void AccountConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("AccountConfig");

    NODE_WRITE_INT     ( this_node, priority);
    NODE_WRITE_STRING  ( this_node, idUri);
    NODE_WRITE_OBJ     ( this_node, regConfig);
    NODE_WRITE_OBJ     ( this_node, sipConfig);
    NODE_WRITE_OBJ     ( this_node, callConfig);
    NODE_WRITE_OBJ     ( this_node, presConfig);
    NODE_WRITE_OBJ     ( this_node, mwiConfig);
    NODE_WRITE_OBJ     ( this_node, natConfig);
    NODE_WRITE_OBJ     ( this_node, mediaConfig);
    NODE_WRITE_OBJ     ( this_node, videoConfig);
}


///////////////////////////////////////////////////////////////////////////////

AccountPresenceStatus::AccountPresenceStatus()
: isOnline(false), activity(PJRPID_ACTIVITY_UNKNOWN)
{
}

///////////////////////////////////////////////////////////////////////////////

void AccountInfo::fromPj(const pjsua_acc_info &pai)
{
    id 			= pai.id;
    isDefault 		= pai.is_default != 0;
    uri			= pj2Str(pai.acc_uri);
    regIsConfigured	= pai.has_registration != 0;
    regIsActive		= pai.has_registration && pai.expires > 0 &&
			    (pai.status / 100 == 2);
    regExpiresSec	= pai.expires;
    regStatus		= pai.status;
    regStatusText	= pj2Str(pai.status_text);
    regLastErr		= pai.reg_last_err;
    onlineStatus	= pai.online_status != 0;
    onlineStatusText	= pj2Str(pai.online_status_text);
}

///////////////////////////////////////////////////////////////////////////////

Account::Account()
: id(PJSUA_INVALID_ID)
{
}

Account::~Account()
{
    /* If this instance is deleted, also delete the corresponding account in
     * PJSUA library.
     */
    if (isValid() && pjsua_get_state() < PJSUA_STATE_CLOSING) {
	PJSUA2_CHECK_EXPR( pjsua_acc_set_user_data(id, NULL) );
	PJSUA2_CHECK_EXPR( pjsua_acc_del(id) );
    }
}

void Account::create(const AccountConfig &acc_cfg,
                     bool make_default) throw(Error)
{
    pjsua_acc_config pj_acc_cfg = acc_cfg.toPj();

    pj_acc_cfg.user_data = (void*)this;
    PJSUA2_CHECK_EXPR( pjsua_acc_add(&pj_acc_cfg, make_default, &id) );
}

void Account::modify(const AccountConfig &acc_cfg) throw(Error)
{
    pjsua_acc_config pj_acc_cfg = acc_cfg.toPj();
    pj_acc_cfg.user_data = (void*)this;
    PJSUA2_CHECK_EXPR( pjsua_acc_modify(id, &pj_acc_cfg) );
}

bool Account::isValid() const
{
    return pjsua_acc_is_valid(id) != 0;
}

void Account::setDefault() throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_acc_set_default(id) );
}

bool Account::isDefault() const
{
    return pjsua_acc_get_default() == id;
}

int Account::getId() const
{
    return id;
}

Account *Account::lookup(int acc_id)
{
    return (Account*)pjsua_acc_get_user_data(acc_id);
}

AccountInfo Account::getInfo() const throw(Error)
{
    pjsua_acc_info pj_ai;
    AccountInfo ai;

    PJSUA2_CHECK_EXPR( pjsua_acc_get_info(id, &pj_ai) );
    ai.fromPj(pj_ai);
    return ai;
}

void Account::setRegistration(bool renew) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_acc_set_registration(id, renew) );
}

void
Account::setOnlineStatus(const AccountPresenceStatus &pres_st) throw(Error)
{
    pjrpid_element pj_rpid;

    pj_bzero(&pj_rpid, sizeof(pj_rpid));
    pj_rpid.type	= PJRPID_ELEMENT_TYPE_PERSON;
    pj_rpid.activity	= pres_st.activity;
    pj_rpid.id		= str2Pj(pres_st.rpidId);
    pj_rpid.note	= str2Pj(pres_st.note);

    PJSUA2_CHECK_EXPR( pjsua_acc_set_online_status2(id, pres_st.isOnline,
                                                    &pj_rpid) );
}

void Account::setTransport(TransportId tp_id) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_acc_set_transport(id, tp_id) );
}

