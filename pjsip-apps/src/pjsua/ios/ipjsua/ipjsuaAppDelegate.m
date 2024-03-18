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
#import <CallKit/CallKit.h>

#include "../../pjsua_app.h"
#include "../../pjsua_app_config.h"

#import "ipjsuaViewController.h"
#import "Reachability.h"

@implementation ipjsuaAppDelegate

#define THIS_FILE       "ipjsuaAppDelegate.m"

/* Specify if we use push notification. */
#define USE_PUSH_NOTIFICATION 1

/* Account details. */
#define SIP_DOMAIN "sip.pjsip.org"
#define SIP_USER   "test"
#define SIP_PASSWD "test"

#define KEEP_ALIVE_INTERVAL 600

ipjsuaAppDelegate      *app;
static pjsua_app_cfg_t  app_cfg;
static bool             isShuttingDown;
static char           **restartArgv;
static int              restartArgc;
Reachability            *internetReach;

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
        pjsua_ip_change_param param;
        pjsua_ip_change_param_default(&param);
//        pjsua_handle_ip_change(&param);
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
    NSLog(@"== VOIP Push Notification Token: %@ ==", self.token);

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

- (void)reportIncomingCall {
    CXCallUpdate *callUpdate = [[CXCallUpdate alloc] init];

    /* Report the incoming call to the system using CallKit. */
    CXProviderConfiguration *configuration = [[CXProviderConfiguration alloc]
                                              initWithLocalizedName:@"ipjsua"];
    CXProvider *provider = [[CXProvider alloc]
                            initWithConfiguration:configuration];

    [provider reportNewIncomingCallWithUUID:[NSUUID UUID] update:callUpdate
              completion:^(NSError * _Nullable error)
    {
        if (error) {
            NSLog(@"Error reporting incoming call: %@",
                  error.localizedDescription);
        } else {
            NSLog(@"Incoming call reported successfully");
        }
    }];
}

- (void)pushRegistry:(PKPushRegistry *)registry
        didReceiveIncomingPushWithPayload:(PKPushPayload *)payload
        forType:(PKPushType)type withCompletionHandler:(void (^)(void))completion
{
    /* Handle incoming VoIP push notification. */
    NSLog(@"Receiving push notification");
    /* Re-register. */
    [self performSelectorOnMainThread:@selector(keepAlive) withObject:nil waitUntilDone:YES];
    /* Report the incoming call via CallKit. */
    [self reportIncomingCall];

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
    
#if !USE_PUSH_NOTIFICATION
    /* Start pjsua app thread immediately, otherwise we do it after push
     * notification setup completes.
     */
    [NSThread detachNewThreadSelector:@selector(pjsuaStart) toTarget:self withObject:nil];
#endif

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)orientationChanged:(NSNotification *)note
{
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
    static pj_thread_desc a_thread_desc;
    static pj_thread_t *a_thread;
    static UIDeviceOrientation prev_ori = 0;
    UIDeviceOrientation dev_ori = [[UIDevice currentDevice] orientation];
    int i;
    
    if (dev_ori == prev_ori) return;
    
    NSLog(@"Device orientation changed: %d", (int)(prev_ori = dev_ori));
    
    if (dev_ori >= UIDeviceOrientationPortrait &&
        dev_ori <= UIDeviceOrientationLandscapeRight)
    {
        if (!pj_thread_is_registered()) {
            pj_thread_register("ipjsua", a_thread_desc, &a_thread);
        }
        
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

- (void)keepAlive {
    static pj_thread_desc a_thread_desc;
    static pj_thread_t *a_thread;
    int i;
    
    if (!pj_thread_is_registered()) {
        pj_thread_register("ipjsua", a_thread_desc, &a_thread);
    }
    
    for (i = 0; i < (int)pjsua_acc_get_count(); ++i) {
        if (pjsua_acc_is_valid(i)) {
            pjsua_acc_set_registration(i, PJ_TRUE);
        }
    }
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    [self performSelectorOnMainThread:@selector(keepAlive) withObject:nil waitUntilDone:YES];

#if 0
    /* setKeepAliveTimeout is deprecated. Use PushKit instead. */
    [application setKeepAliveTimeout:KEEP_ALIVE_INTERVAL handler: ^{
        [self performSelectorOnMainThread:@selector(keepAlive) withObject:nil waitUntilDone:YES];
    }];
#endif
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


pj_bool_t showNotification(pjsua_call_id call_id)
{
    /* This is deprecated. Use VoIP Push Notifications with PushKit
     * framework instead.
     */
#if 0
    // Create a new notification
    UILocalNotification* alert = [[UILocalNotification alloc] init];
    if (alert)
    {
        alert.repeatInterval = 0;
        alert.alertBody = @"Incoming call received...";
        /* This action just brings the app to the FG, it doesn't
         * automatically answer the call (unless you specify the
         * --auto-answer option).
         */
        alert.alertAction = @"Activate app";
        
        dispatch_async(dispatch_get_main_queue(),
                       ^{[[UIApplication sharedApplication]
                          presentLocalNotificationNow:alert];});
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
