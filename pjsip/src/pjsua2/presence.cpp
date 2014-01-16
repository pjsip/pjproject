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
#include <pjsua2/presence.hpp>
#include <pjsua2/account.hpp>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE		"presence.cpp"


///////////////////////////////////////////////////////////////////////////////

PresenceStatus::PresenceStatus()
: status(PJSUA_BUDDY_STATUS_UNKNOWN), activity(PJRPID_ACTIVITY_UNKNOWN)
{
}


///////////////////////////////////////////////////////////////////////////////

void BuddyConfig::readObject(const ContainerNode &node) throw(Error)
{
    ContainerNode this_node = node.readContainer("BuddyConfig");

    NODE_READ_STRING   ( this_node, uri);
    NODE_READ_BOOL     ( this_node, subscribe);
}

void BuddyConfig::writeObject(ContainerNode &node) const throw(Error)
{
    ContainerNode this_node = node.writeNewContainer("BuddyConfig");

    NODE_WRITE_STRING  ( this_node, uri);
    NODE_WRITE_BOOL    ( this_node, subscribe);
}

//////////////////////////////////////////////////////////////////////////////

void BuddyInfo::fromPj(const pjsua_buddy_info &pbi)
{
    uri 		= pj2Str(pbi.uri);
    contact 		= pj2Str(pbi.contact);
    presMonitorEnabled 	= PJ2BOOL(pbi.monitor_pres);
    subState 		= pbi.sub_state;
    subStateName 	= string(pbi.sub_state_name);
    subTermCode 	= (pjsip_status_code)pbi.sub_term_code;
    subTermReason 	= pj2Str(pbi.sub_term_reason);
    
    /* Presence status */
    presStatus.status	= pbi.status;
    presStatus.statusText = pj2Str(pbi.status_text);
    presStatus.activity = pbi.rpid.activity;
    presStatus.note	= pj2Str(pbi.rpid.note);
    presStatus.rpidId	= pj2Str(pbi.rpid.id);
}

//////////////////////////////////////////////////////////////////////////////

/*
 * Constructor.
 */
Buddy::Buddy()
: id(PJSUA_INVALID_ID)
{
}
 
/*
 * Destructor.
 */
Buddy::~Buddy()
{
    if (isValid()) {
	pjsua_buddy_set_user_data(id, NULL);
	pjsua_buddy_del(id);

	/* Remove from account buddy list */
	acc->removeBuddy(this);
    }
}
    
/*
 * Create buddy and register the buddy to PJSUA-LIB.
 */
void Buddy::create(Account &account, const BuddyConfig &cfg) throw(Error)
{
    pjsua_buddy_config pj_cfg;
    pjsua_buddy_config_default(&pj_cfg);
    
    if (!account.isValid())
	PJSUA2_RAISE_ERROR3(PJ_EINVAL, "Buddy::create()", "Invalid account");
    
    pj_cfg.uri = str2Pj(cfg.uri);
    pj_cfg.subscribe = cfg.subscribe;
    pj_cfg.user_data = (void*)this;
    PJSUA2_CHECK_EXPR( pjsua_buddy_add(&pj_cfg, &id) );
    
    acc = &account;
    acc->addBuddy(this);
}
    
/*
 * Check if this buddy is valid.
 */
bool Buddy::isValid() const
{
    return PJ2BOOL( pjsua_buddy_is_valid(id) );
}

/*
 * Get detailed buddy info.
 */
BuddyInfo Buddy::getInfo() const throw(Error)
{
    pjsua_buddy_info pj_bi;
    BuddyInfo bi;

    PJSUA2_CHECK_EXPR( pjsua_buddy_get_info(id, &pj_bi) );
    bi.fromPj(pj_bi);
    return bi;
}

/*
 * Enable/disable buddy's presence monitoring.
 */
void Buddy::subscribePresence(bool subscribe) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_buddy_subscribe_pres(id, subscribe) );
}

    
/*
 * Update the presence information for the buddy.
 */
void Buddy::updatePresence(void) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_buddy_update_pres(id) );
}
     
/*
 * Send instant messaging outside dialog.
 */
void Buddy::sendInstantMessage(const SendInstantMessageParam &prm) throw(Error)
{
    BuddyInfo bi = getInfo();

    pj_str_t to = str2Pj(bi.contact.empty()? bi.uri : bi.contact);
    pj_str_t mime_type = str2Pj(prm.contentType);
    pj_str_t content = str2Pj(prm.content);
    void *user_data = (void*)prm.userData;
    pjsua_msg_data msg_data;
    prm.txOption.toPj(msg_data);
    
    PJSUA2_CHECK_EXPR( pjsua_im_send(acc->getId(), &to, &mime_type, &content,
				     &msg_data, user_data) );
}

/*
 * Send typing indication outside dialog.
 */
void Buddy::sendTypingIndication(const SendTypingIndicationParam &prm)
     throw(Error)
{
    BuddyInfo bi = getInfo();

    pj_str_t to = str2Pj(bi.contact.empty()? bi.uri : bi.contact);
    pjsua_msg_data msg_data;
    prm.txOption.toPj(msg_data);
    
    PJSUA2_CHECK_EXPR( pjsua_im_typing(acc->getId(), &to, prm.isTyping,
				       &msg_data) );
}

