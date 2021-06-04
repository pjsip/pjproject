/*
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
 * Contributed by Emre Tufekci (github.com/emretufekci)
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

#import <Foundation/Foundation.h>

/**
 Create a object to be able to use it from C++
 */
@interface CPPWrapper : NSObject

//Lib
/**
 Create Lib with EpConfig
 */
-(void) createLibWrapper;

/**
 Delete lib
 */
-(void) deleteLibWrapper;


//Account
/**
 Create Account via following config(string username, string password, string ip, string port)
 */
-(void) createAccountWrapper :(NSString*) username :(NSString*) password :(NSString*) ip :(NSString*) port;

/**
 Unregister account
 */
-(void) unregisterAccountWrapper;



//Register State Info

-(bool) registerStateInfoWrapper;



//Call
/**
 Get caller id for incoming call, checks account currently registered (ai.regIsActive)
 */
-(NSString*) incomingCallInfoWrapper;


/**
 Listener (When we have incoming call, this function pointer will notify swift.)
 (Runs swift code from C++)
 */
-(void) incoming_call_wrapper: (void(*)())function;

/**
 Listener (When we have changes on the call state, this function pointer will notify swift.)
 (Runs swift code from C++)
 */
-(void) call_listener_wrapper: (void(*)(int))function;

/**
 Answer incoming call
 */
-(void) answerCallWrapper;

/**
 Hangup active call (Incoming/Outgoing/Active)
 */
-(void) hangupCallWrapper;

/**
 Hold the call
 */
-(void) holdCallWrapper;

/**
 unhold the call
 */
-(void) unholdCallWrapper;

/**
 Make outgoing call (string dest_uri) -> e.g. makeCall(sip:<SIP_USERNAME@SIP_IP:SIP_PORT>)
 */
-(void) outgoingCallWrapper :(NSString*) dest_uri;
@end
