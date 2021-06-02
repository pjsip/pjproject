//
//  wrapper.hpp
//  ios-swift-pjsua2
//
//  Created by Emre on 7.01.2021.
//

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
