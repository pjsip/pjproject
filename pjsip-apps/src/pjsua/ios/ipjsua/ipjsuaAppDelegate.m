//
//  ipjsuaAppDelegate.m
//  ipjsua
/*
 * Copyright (C) 2013-2014 Teluu Inc. (http://www.teluu.com)
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
#import "ipjsuaAppDelegate.h"
#import <pjlib.h>
#import <pjsua.h>
#import <pj/log.h>
#import <AVFoundation/AVFoundation.h>
#import <CallKit/CallKit.h>

#include "../../pjsua_app.h"
#include "../../pjsua_app_config.h"

#import "ipjsuaViewController.h"
#import "Reachability.h"

@implementation ipjsuaAppDelegate

#define THIS_FILE       "ipjsuaAppDelegate.m"

/* Specify if we use push notification. */
#define USE_PUSH_NOTIFICATION 1

/* Specify the timeout (in sec) to wait for the incoming INVITE
 * to come after we receive push notification.
 */
#define MAX_INV_TIMEOUT 10

/* Account details. */
#define SIP_DOMAIN "sip.pjsip.org"
#define SIP_USER   "test"
#define SIP_PASSWD "test"

ipjsuaAppDelegate      *app;
static pjsua_app_cfg_t  app_cfg;
static bool             isShuttingDown;
static char           **restartArgv;
static int              restartArgc;
Reachability           *internetReach;

/* Mapping between CallKit UUID and pjsua_call_id. */
NSMutableDictionary<NSUUID *, NSNumber *> *call_map;

enum {
    REREGISTER = 1,
    ANSWER_CALL,
    END_CALL,
    ACTIVATE_AUDIO,
    DEACTIVATE_AUDIO,
    HANDLE_IP_CHANGE,
    HANDLE_ORI_CHANGE
};

#define REGISTER_THREAD \
    static pj_thread_desc a_thread_desc; \
    static pj_thread_t *a_thread; \
    if (!pj_thread_is_registered()) { \
        pj_thread_register("ipjsua", a_thread_desc, &a_thread); \
    }

#define SCHEDULE_TIMER(action) \
{ \
    REGISTER_THREAD \
    pjsua_schedule_timer2(pjsip_funcs, (void *)action, 0); \
}

static void pjsip_funcs(void *user_data)
{
    /* IMPORTANT:
     * We need to call PJSIP API from a separate thread since
     * PJSIP API can potentially block the main/GUI thread.
     * And make sure we don't use Apple's Dispatch / gcd since
     * it's incompatible with POSIX threads.
     * In this example, we take advantage of PJSUA's timer thread
     * to invoke PJSIP APIs. For a more complex application,
     * it is recommended to create your own separate thread
     * instead for this purpose.
     */
    long code = (long)user_data & 0xF;
    if (code == REREGISTER) {
        for (unsigned i = 0; i < pjsua_acc_get_count(); ++i) {
            if (pjsua_acc_is_valid(i)) {
                pjsua_acc_set_registration(i, PJ_TRUE);
            }
        }
    } else if (code == ANSWER_CALL) {
        pj_status_t status;
        pjsua_call_id call_id = (pjsua_call_id)((long)user_data & 0xFF0) >> 4;

        status = pjsua_call_answer(call_id, PJSIP_SC_OK, NULL, NULL);
        if (status != PJ_SUCCESS) {
            NSUUID *uuid =(__bridge NSUUID *)pjsua_call_get_user_data(call_id);
            if (uuid) {
                [app.provider reportCallWithUUID:uuid
                                     endedAtDate:[NSDate date]
                                          reason:CXCallEndedReasonFailed];
            }
        }
    } else if (code == END_CALL) {
        pj_status_t status;
        pjsua_call_id call_id = (pjsua_call_id)((long)user_data & 0xFF0) >> 4;

        status = pjsua_call_hangup(call_id, PJSIP_SC_OK, NULL, NULL);
        if (status != PJ_SUCCESS) {
            NSUUID *uuid =(__bridge NSUUID *)pjsua_call_get_user_data(call_id);
            if (uuid) {
                [app.provider reportCallWithUUID:uuid
                                     endedAtDate:[NSDate date]
                                          reason:CXCallEndedReasonFailed];
            }
        }
    } else if (code == ACTIVATE_AUDIO) {
        pjsua_call_info call_info;
        pjsua_call_id ids[PJSUA_MAX_CALLS];
        unsigned count = PJSUA_MAX_CALLS;

        /* If we use CallKit, sound device may not work until audio session
         * becomes active, so we need to force reopen sound device here.
         */
        pjsua_set_no_snd_dev();
        pjsua_set_snd_dev(PJSUA_SND_DEFAULT_CAPTURE_DEV,
                          PJSUA_SND_DEFAULT_PLAYBACK_DEV);

        pjsua_enum_calls(ids, &count);
        for (unsigned i = 0; i < count; i++) {
            pjsua_call_get_info(i, &call_info);

            for (unsigned mi = 0; mi < call_info.media_cnt; ++mi) {
                if (call_info.media[mi].type == PJMEDIA_TYPE_AUDIO &&
                    (call_info.media[mi].status == PJSUA_CALL_MEDIA_ACTIVE ||
                     call_info.media[mi].status == PJSUA_CALL_MEDIA_REMOTE_HOLD))
                {
                    pjsua_conf_port_id call_conf_slot;
                    call_conf_slot = call_info.media[mi].stream.aud.conf_slot;
                    pjsua_conf_connect(0, call_conf_slot);
                    pjsua_conf_connect(call_conf_slot, 0);
                }
            }
        }
    } else if (code == DEACTIVATE_AUDIO) {
        NSLog(@"Deactivating audio session, is sound active: %d",
              pjsua_snd_is_active());
        if (!pjsua_snd_is_active()) {
            [[AVAudioSession sharedInstance] setActive:NO
            withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
            error:nil];
        }
    } else if (code == HANDLE_IP_CHANGE) {
        pjsua_ip_change_param param;
        pjsua_ip_change_param_default(&param);
        pjsua_handle_ip_change(&param);
    } else if (code == HANDLE_ORI_CHANGE) {
#if PJSUA_HAS_VIDEO
        const pjmedia_orient pj_ori[4] =
        {
            PJMEDIA_ORIENT_ROTATE_90DEG,  /* UIDeviceOrientationPortrait */
            PJMEDIA_ORIENT_ROTATE_270DEG, /* UIDeviceOrientationPortraitUpsideDown */
            PJMEDIA_ORIENT_ROTATE_180DEG, /* UIDeviceOrientationLandscapeLeft,
                                             home button on the right side */
            PJMEDIA_ORIENT_NATURAL        /* UIDeviceOrientationLandscapeRight,
                                             home button on the left side */
        };
        static UIDeviceOrientation prev_ori = 0;
        UIDeviceOrientation dev_ori = [[UIDevice currentDevice] orientation];
        int i;

        if (dev_ori == prev_ori) return;

        NSLog(@"Device orientation changed: %d", (int)(prev_ori = dev_ori));

        if (dev_ori >= UIDeviceOrientationPortrait &&
            dev_ori <= UIDeviceOrientationLandscapeRight)
        {
            /* Here we set the orientation for all video devices.
             * This may return failure for renderer devices or for
             * capture devices which do not support orientation setting,
             * we can simply ignore them.
             */
            for (i = pjsua_vid_dev_count()-1; i >= 0; i--) {
                pjsua_vid_dev_set_setting(i, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                                          &pj_ori[dev_ori-1], PJ_TRUE);
            }
        }
#endif
    }
}

- (void) updateWithReachability: (Reachability *)curReach
{
    NetworkStatus netStatus = [curReach currentReachabilityStatus];
    BOOL connectionRequired = [curReach connectionRequired];
    switch (netStatus) {
        case NotReachable:
            NSLog(@"Access Not Available..");
            connectionRequired= NO;
            break;
        case ReachableViaWiFi:
            NSLog(@"Reachable WiFi..");
            break;
        case ReachableViaWWAN:
            NSLog(@"Reachable WWAN..");
        break;
    }
    if (connectionRequired) {
        NSLog(@"Connection Required");
    }
}

/* Called by Reachability whenever status changes. */
- (void)reachabilityChanged: (NSNotification *)note
{
    Reachability* curReach = [note object];
    NSParameterAssert([curReach isKindOfClass: [Reachability class]]);
    NSLog(@"reachability changed..");
    [self updateWithReachability: curReach];
    
    if ([curReach currentReachabilityStatus] != NotReachable &&
        ![curReach connectionRequired])
    {
        SCHEDULE_TIMER(HANDLE_IP_CHANGE);
    }
}

- (void)pushRegistry:(PKPushRegistry *)registry didUpdatePushCredentials:(PKPushCredentials *)credentials forType:(NSString *)type
{
    if ([credentials.token length] == 0) {
        NSLog(@"voip token NULL");
        return;
    }

    /* Get device token */
    const char *data = [credentials.token bytes];
    self.token = [NSMutableString string];
    for (NSUInteger i = 0; i < [credentials.token length]; i++) {
        [self.token appendFormat:@"%02.2hhx", data[i]];
    }
    NSLog(@"VOIP Push Notification Token: %@", self.token);

    /* Now start pjsua */
    [NSThread detachNewThreadSelector:@selector(pjsuaStart) toTarget:self withObject:nil];
}

void displayLog(const char *msg, int len)
{
    NSLog(@"%.*s", len, msg);
}

static void displayMsg(const char *msg)
{
    NSString *str = [NSString stringWithFormat:@"%s", msg];
    dispatch_async(dispatch_get_main_queue(),
                   ^{app.viewController.textLabel.text = str;});
}

static void pjsuaOnStartedCb(pj_status_t status, const char* msg)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    
    if (status != PJ_SUCCESS && (!msg || !*msg)) {
        pj_strerror(status, errmsg, sizeof(errmsg));
        PJ_LOG(3,(THIS_FILE, "Error: %s", errmsg));
        msg = errmsg;
    } else {
        PJ_LOG(3,(THIS_FILE, "Started: %s", msg));
    }

    displayMsg(msg);
}

static void pjsuaOnStoppedCb(pj_bool_t restart,
                             int argc, char** argv)
{
    PJ_LOG(3,("ipjsua", "CLI %s request", (restart? "restart" : "shutdown")));
    if (restart) {
        displayMsg("Restarting..");
        pj_thread_sleep(100);
        app_cfg.argc = argc;
        app_cfg.argv = argv;
    } else {
        displayMsg("Shutting down..");
        pj_thread_sleep(100);
        isShuttingDown = true;
    }
}

static void pjsuaOnAppConfigCb(pjsua_app_config *cfg)
{
    PJ_UNUSED_ARG(cfg);
}

- (void)pjsuaStart
{
    // TODO: read from config?
    const char **argv = pjsua_app_def_argv;
    int argc = PJ_ARRAY_SIZE(pjsua_app_def_argv) -1;
    pj_status_t status;
    
    isShuttingDown = false;
    displayMsg("Starting..");
    
    pj_bzero(&app_cfg, sizeof(app_cfg));
    if (restartArgc) {
        app_cfg.argc = restartArgc;
        app_cfg.argv = restartArgv;
    } else {
        app_cfg.argc = argc;
        app_cfg.argv = (char**)argv;
    }
    app_cfg.on_started = &pjsuaOnStartedCb;
    app_cfg.on_stopped = &pjsuaOnStoppedCb;
    app_cfg.on_config_init = &pjsuaOnAppConfigCb;
    
    while (!isShuttingDown) {
        status = pjsua_app_init(&app_cfg);
        if (status != PJ_SUCCESS) {
            char errmsg[PJ_ERR_MSG_SIZE];
            pj_strerror(status, errmsg, sizeof(errmsg));
            displayMsg(errmsg);
            pjsua_app_destroy();
            return;
        }
    
        /* Setup device orientation change notification */
        dispatch_async(dispatch_get_main_queue(),
        ^{
            [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
            [[NSNotificationCenter defaultCenter] addObserver:app
                selector:@selector(orientationChanged:)
                name:UIDeviceOrientationDidChangeNotification
                object:[UIDevice currentDevice]];
        });
        
        static char contact_uri_buf[1024];
        pjsua_acc_config cfg;

        pjsua_acc_config_default(&cfg);
        cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
        cfg.reg_uri = pj_str("sip:" SIP_DOMAIN ";transport=tcp");
        cfg.cred_count = 1;
        cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
        cfg.cred_info[0].scheme = pj_str("digest");
        cfg.cred_info[0].username = pj_str(SIP_USER);
        cfg.cred_info[0].data = pj_str(SIP_PASSWD);
        /* Uncomment this to enable video. Note that video can only
         * work in the foreground.
         */
        // app_config_init_video(&cfg);

        /* If we have Push Notification token, put it in the registration
         * Contact URI params.
         */
        if ([self.token length]) {
            /* According to RFC 8599:
             * - pn-provider is the Push Notification Service provider. Here
             * we use APNS (Apple Push Notification Service).
             * - pn-param is composed of Team ID and Topic separated by
             * a period (.).
             * The Topic consists of the Bundle ID, the app's unique ID,
             * and the app's service value ("voip" for VoIP apps), separated
             * by a period (.).
             * - pn-prid is the PN token.
             */
            NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
            NSString *bundleID = infoDictionary[@"CFBundleIdentifier"];
            NSString *teamID = infoDictionary[@"AppIdentifierPrefix"];
            NSLog(@"Team ID from settings: %@", teamID);

            pj_ansi_snprintf(contact_uri_buf, sizeof(contact_uri_buf),
                             ";pn-provider=apns"
                             ";pn-param=%s%s.voip"
                             ";pn-prid=%s",
                             (teamID? [teamID UTF8String]: "93NHJQ455P."),
                             [bundleID UTF8String],
                             [self.token UTF8String]);
            cfg.reg_contact_uri_params = pj_str(contact_uri_buf);
        }
        status = pjsua_acc_add(&cfg, PJ_TRUE, NULL);
        if (status != PJ_SUCCESS) {
            char errmsg[PJ_ERR_MSG_SIZE];
            pj_strerror(status, errmsg, sizeof(errmsg));
            displayMsg(errmsg);
        }

        status = pjsua_app_run(PJ_TRUE);
        if (status != PJ_SUCCESS) {
            char errmsg[PJ_ERR_MSG_SIZE];
            pj_strerror(status, errmsg, sizeof(errmsg));
            displayMsg(errmsg);
        }
        
        [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
    
        pjsua_app_destroy();
    }
    
    restartArgv = NULL;
    restartArgc = 0;
}

- (void) provider:(CXProvider *) provider
didActivateAudioSession:(AVAudioSession *) audioSession
{
    NSLog(@"Did activate Audio Session");

    pjsua_schedule_timer2(pjsip_funcs, (void *)ACTIVATE_AUDIO, 0);
}

- (void) provider:(CXProvider *) provider
didDectivateAudioSession:(AVAudioSession *) audioSession
{
    NSLog(@"Did deactivate Audio Session");
}

- (void)provider:(CXProvider *)provider
        performAnswerCallAction:(CXAnswerCallAction *)action
{
    NSLog(@"Perform answer call %@", action.callUUID.UUIDString);

    /* User has answered the call, but we may need to wait for
     * the incoming INVITE to come.
     */
    for (int i = MAX_INV_TIMEOUT * 1000 / 100; i >= 0; i--) {
        if (call_map[action.callUUID].intValue != PJSUA_INVALID_ID) break;
        [NSThread sleepForTimeInterval:0.1];
    }

    [action fulfill];

    pjsua_call_id call_id = call_map[action.callUUID].intValue;
    if (call_id == PJSUA_INVALID_ID) {
        [app.provider reportCallWithUUID:action.callUUID
                      endedAtDate:[NSDate date]
                      reason:CXCallEndedReasonFailed];
        return;
    }

    long code = (long)ANSWER_CALL | (call_id << 4);
    pjsua_schedule_timer2(pjsip_funcs, (void *)code, 0);
}

- (void)provider:(CXProvider *)provider
        performEndCallAction:(CXEndCallAction *)action
{
    NSLog(@"Perform end call %@", action.callUUID.UUIDString);

    /* User has answered the call, but we may need to wait for
     * the incoming INVITE to come.
     */
    for (int i = MAX_INV_TIMEOUT * 1000 / 100; i >= 0; i--) {
        if (call_map[action.callUUID].intValue != PJSUA_INVALID_ID) break;
        [NSThread sleepForTimeInterval:0.1];
    }

    [action fulfill];

    pjsua_call_id call_id = call_map[action.callUUID].intValue;
    if (call_id == PJSUA_INVALID_ID) {
        [app.provider reportCallWithUUID:action.callUUID
                      endedAtDate:[NSDate date]
                      reason:CXCallEndedReasonFailed];
        return;
    }

    long code = (long)END_CALL | (call_id << 4);
    pjsua_schedule_timer2(pjsip_funcs, (void *)code, 0);
}

- (void)pushRegistry:(PKPushRegistry *)registry
        didReceiveIncomingPushWithPayload:(PKPushPayload *)payload
        forType:(PKPushType)type withCompletionHandler:(void (^)(void))completion
{
    /* Handle incoming VoIP push notification. */
    NSUUID *uuid = [NSUUID UUID];
    call_map[uuid] = @(PJSUA_INVALID_ID);
    NSLog(@"Receiving push notification %@", uuid.UUIDString);

    /* Re-register, so the server will send us the suspended INVITE. */
    SCHEDULE_TIMER(REREGISTER);

    /* Activate audio session. */
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    NSError *error = nil;
    if (![audioSession setCategory:AVAudioSessionCategoryPlayAndRecord
                       mode:AVAudioSessionModeVoiceChat
                       options:0 error:&error])
    {
        NSLog(@"Error setting up audio session: %@", error.localizedDescription);
    }
    if (![audioSession setActive:YES error:&error]) {
        NSLog(@"Error activating audio session: %@", error.localizedDescription);
    }

    /* Report the incoming call to the system using CallKit. */
    CXCallUpdate *callUpdate = [[CXCallUpdate alloc] init];
    [self.provider reportNewIncomingCallWithUUID:uuid
                   update:callUpdate
                   completion:^(NSError * _Nullable error)
    {
        if (error) {
            NSLog(@"Error reporting incoming call: %@",
                  error.localizedDescription);
            [call_map removeObjectForKey:uuid];
        }
    }];

    /* Call the completion handler when you have finished processing the incoming call. */
    completion();
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    // Override point for customization after application launch.
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        self.viewController = [[ipjsuaViewController alloc] initWithNibName:@"ipjsuaViewController_iPhone" bundle:nil];
    } else {
        self.viewController = [[ipjsuaViewController alloc] initWithNibName:@"ipjsuaViewController_iPad" bundle:nil];
    }
    self.window.rootViewController = self.viewController;
    [self.window makeKeyAndVisible];

#if USE_PUSH_NOTIFICATION
    call_map = [NSMutableDictionary dictionary];

    /* Set up a push notification registry for Voice over IP (VoIP). */
    self.voipRegistry = [[PKPushRegistry alloc] initWithQueue:dispatch_get_main_queue()];
    self.voipRegistry.delegate = self;
    self.voipRegistry.desiredPushTypes = [NSSet setWithObject:PKPushTypeVoIP];

    /* Request permission for push notifications. */
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
    center.delegate = self;
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert |
                                             UNAuthorizationOptionBadge |
                                             UNAuthorizationOptionSound)
            completionHandler:^(BOOL granted, NSError * _Nullable error)
    {
        NSLog(@"Notification request %sgranted", (granted ? "" : "not"));

        if (granted) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [application registerForRemoteNotifications];
            });
        }
    }];

    /* Note that opening audio device when in the background will not trigger
     * permission request, so we won't be able to use audio device.
     * Thus, we need to request permission now.
     */
    [[AVAudioSession sharedInstance] requestRecordPermission:^(BOOL granted)
    {
        NSLog(@"Microphone access %sgranted", (granted ? "" : "not"));
    }];

    /* We need to request local network access permission as well to
     * immediately send media traffic when in the background.
     * Due to its complexity, the code is not included here in the sample app.
     * Please refer to the Apple "Local Network Privacy" FAQ for more details.
     */

    /* Create CallKit provider. */
    CXProviderConfiguration *configuration = [[CXProviderConfiguration alloc]
                                              initWithLocalizedName:@"ipjsua"];
    configuration.maximumCallGroups = 1;
    configuration.maximumCallsPerCallGroup = 1;
    self.provider = [[CXProvider alloc] initWithConfiguration:configuration];
    [self.provider setDelegate:self queue:nil];
#else
    /* Start pjsua app thread immediately, otherwise we do it after push
     * notification setup completes.
     */
    [NSThread detachNewThreadSelector:@selector(pjsuaStart) toTarget:self
                                      withObject:nil];
#endif

    /* Observe the kNetworkReachabilityChangedNotification. When that
     * notification is posted, the method "reachabilityChanged" will be called.
     */
    [[NSNotificationCenter defaultCenter] addObserver: self
          selector: @selector(reachabilityChanged:)
          name: kReachabilityChangedNotification object: nil];
    
    internetReach = [Reachability reachabilityForInternetConnection];
    [internetReach startNotifier];
    [self updateWithReachability: internetReach];
    
    app = self;

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)orientationChanged:(NSNotification *)note
{
    SCHEDULE_TIMER(HANDLE_ORI_CHANGE);
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    SCHEDULE_TIMER(REREGISTER);
    /* Allow the re-registration to complete. */
    [NSThread sleepForTimeInterval:0.3];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}


pj_bool_t reportCallState(pjsua_call_id call_id)
{
    pjsua_call_info info;

    pjsua_call_get_info(call_id, &info);

    if (info.state == PJSIP_INV_STATE_DISCONNECTED) {
        NSUUID *uuid = (__bridge NSUUID *)pjsua_call_get_user_data(call_id);
        if (uuid && call_map[uuid].intValue == call_id) {
            [app.provider reportCallWithUUID:uuid
                          endedAtDate:[NSDate date]
                          reason:CXCallEndedReasonRemoteEnded];
            [call_map removeObjectForKey:uuid];
        }

        /* Check if we need to deactivate audio session. Note that sound device
         * will only be closed after pjsua_media_config.snd_auto_close_time.
         */
        pjsua_schedule_timer2(pjsip_funcs, (void *)DEACTIVATE_AUDIO, 1500);
    }

    return PJ_FALSE;
}

pj_bool_t showNotification(pjsua_call_id call_id)
{
#if USE_PUSH_NOTIFICATION
    NSLog(@"Receiving incoming call %d", call_id);

    for (NSUUID *uuid in call_map) {
        if (call_map[uuid].intValue == PJSUA_INVALID_ID) {
            NSLog(@"Associating incoming call %d with UUID %@",
                  call_id, uuid.UUIDString);
            call_map[uuid] = @(call_id);
            pjsua_call_set_user_data(call_id, (__bridge void *)uuid);

            break;
        }
    }
#endif

    return PJ_FALSE;
}

void displayWindow(pjsua_vid_win_id wid)
{
#if PJSUA_HAS_VIDEO
    int i, last;
    
    i = (wid == PJSUA_INVALID_ID) ? 0 : wid;
    last = (wid == PJSUA_INVALID_ID) ? PJSUA_MAX_VID_WINS : wid+1;

    for (;i < last; ++i) {
        pjsua_vid_win_info wi;
        
        if (pjsua_vid_win_get_info(i, &wi) == PJ_SUCCESS &&
            wi.hwnd.info.ios.window)
        {
            dispatch_async(dispatch_get_main_queue(), ^{
                UIView *parent = app.viewController.view;
                UIView *view = (__bridge UIView *)wi.hwnd.info.ios.window;

                if (![view isDescendantOfView:parent])
                    [parent addSubview:view];

                if (!wi.is_native) {
                    /* Video window */
                    view.contentMode = UIViewContentModeScaleAspectFit;
                    view.center = parent.center;
                    view.frame = parent.bounds;
                } else {
                    /* Preview window */
                    view.contentMode = UIViewContentModeBottomLeft;
                    view.frame = CGRectMake(0, parent.frame.size.height - view.frame.size.height,
                                            view.frame.size.width, view.frame.size.height);
                }
            });
        }
    }
#endif
}

@end
