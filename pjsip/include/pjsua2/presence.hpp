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
#ifndef __PJSUA2_PRESENCE_HPP__
#define __PJSUA2_PRESENCE_HPP__

/**
 * @file pjsua2/presence.hpp
 * @brief PJSUA2 Presence Operations
 */
#include <pjsua2/persistent.hpp>
#include <pjsua2/siptypes.hpp>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_PRES Presence
 * @ingroup PJSUA2_Ref
 * @{
 */

using std::string;
using std::vector;


/**
 * This describes presence status.
 */
struct PresenceStatus
{
    /**
     * Buddy's online status.
     */
    pjsua_buddy_status	 status;

    /**
     * Text to describe buddy's online status.
     */
    string		 statusText;
    
    /**
     * Activity type.
     */
    pjrpid_activity	 activity;

    /**
     * Optional text describing the person/element.
     */
    string		 note;

    /**
     * Optional RPID ID string.
     */
    string		 rpidId;

public:
    /**
     * Constructor.
     */
    PresenceStatus();
};


/**
 * This structure describes buddy configuration when adding a buddy to
 * the buddy list with Buddy::create().
 */
struct BuddyConfig : public PersistentObject
{
    /**
     * Buddy URL or name address.
     */
    string	 	 uri;

    /**
     * Specify whether presence subscription should start immediately.
     */
    bool	 	 subscribe;

public:
    /**
     * Read this object from a container node.
     *
     * @param node		Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) throw(Error);

    /**
     * Write this object to a container node.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const throw(Error);
};


/**
 * This structure describes buddy info, which can be retrieved by via
 * Buddy::getInfo().
 */
struct BuddyInfo
{
    /**
     * The full URI of the buddy, as specified in the configuration.
     */
    string		 uri;

    /**
     * Buddy's Contact, only available when presence subscription has
     * been established to the buddy.
     */
    string		 contact;

    /**
     * Flag to indicate that we should monitor the presence information for
     * this buddy (normally yes, unless explicitly disabled).
     */
    bool		 presMonitorEnabled;

    /**
     * If \a presMonitorEnabled is true, this specifies the last state of
     * the presence subscription. If presence subscription session is currently
     * active, the value will be PJSIP_EVSUB_STATE_ACTIVE. If presence
     * subscription request has been rejected, the value will be
     * PJSIP_EVSUB_STATE_TERMINATED, and the termination reason will be
     * specified in \a subTermReason.
     */
    pjsip_evsub_state	 subState;

    /**
     * String representation of subscription state.
     */
    string	         subStateName;

    /**
     * Specifies the last presence subscription termination code. This would
     * return the last status of the SUBSCRIBE request. If the subscription
     * is terminated with NOTIFY by the server, this value will be set to
     * 200, and subscription termination reason will be given in the
     * \a subTermReason field.
     */
    pjsip_status_code	 subTermCode;

    /**
     * Specifies the last presence subscription termination reason. If 
     * presence subscription is currently active, the value will be empty.
     */
    string		 subTermReason;

    /**
     * Presence status.
     */
    PresenceStatus	 presStatus;
    
public:
    /** Import from pjsip structure */
    void fromPj(const pjsua_buddy_info &pbi);
};


/**
 * Buddy.
 */
class Buddy
{
public:
    /**
     * Constructor.
     */
    Buddy();
    
    /**
     * Destructor. Note that if the Buddy instance is deleted, it will also
     * delete the corresponding buddy in the PJSUA-LIB.
     */
    virtual ~Buddy();
    
    /**
     * Create buddy and register the buddy to PJSUA-LIB.
     *
     * @param acc		The account for this buddy.
     * @param cfg		The buddy config.
     */
    void create(Account &acc, const BuddyConfig &cfg) throw(Error);
    
    /**
     * Check if this buddy is valid.
     *
     * @return			True if it is.
     */
    bool isValid() const;

    /**
     * Get detailed buddy info.
     *
     * @return			Buddy info.
     */
    BuddyInfo getInfo() const throw(Error);

    /**
     * Enable/disable buddy's presence monitoring. Once buddy's presence is
     * subscribed, application will be informed about buddy's presence status
     * changed via \a onBuddyState() callback.
     *
     * @param subscribe		Specify true to activate presence
     *				subscription.
     */
    void subscribePresence(bool subscribe) throw(Error);
    
    /**
     * Update the presence information for the buddy. Although the library
     * periodically refreshes the presence subscription for all buddies,
     * some application may want to refresh the buddy's presence subscription
     * immediately, and in this case it can use this function to accomplish
     * this.
     *
     * Note that the buddy's presence subscription will only be initiated
     * if presence monitoring is enabled for the buddy. See
     * subscribePresence() for more info. Also if presence subscription for
     * the buddy is already active, this function will not do anything.
     *
     * Once the presence subscription is activated successfully for the buddy,
     * application will be notified about the buddy's presence status in the
     * \a onBuddyState() callback.
     */
     void updatePresence(void) throw(Error);
     
    /**
     * Send instant messaging outside dialog, using this buddy's specified
     * account for route set and authentication.
     *
     * @param prm	Sending instant message parameter.
     */
    void sendInstantMessage(const SendInstantMessageParam &prm) throw(Error);

    /**
     * Send typing indication outside dialog.
     *
     * @param prm	Sending instant message parameter.
     */
    void sendTypingIndication(const SendTypingIndicationParam &prm)
	 throw(Error);

public:
    /*
     * Callbacks
     */
     
    /**
     * Notify application when the buddy state has changed.
     * Application may then query the buddy info to get the details.
     */
    virtual void onBuddyState()
    {}
     
private:
     /**
      * Buddy ID.
      */
     pjsua_buddy_id	 id;
     
     /**
      * Account.
      */
     Account		*acc;
};


/** Array of buddies */
typedef std::vector<Buddy*> BuddyVector;


/**
 * @}  // PJSUA2_PRES
 */

} // namespace pj

#endif	/* __PJSUA2_PRESENCE_HPP__ */
