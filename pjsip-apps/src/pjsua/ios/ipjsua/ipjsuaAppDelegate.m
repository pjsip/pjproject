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

#include "../../pjsua_app.h"
#include "../../pjsua_app_config.h"

#import "ipjsuaViewController.h"

@implementation ipjsuaAppDelegate

#define THIS_FILE	"ipjsuaAppDelegate.m"

#define KEEP_ALIVE_INTERVAL 600

ipjsuaAppDelegate      *app;
static pjsua_app_cfg_t  app_cfg;
static bool             isShuttingDown;
static char           **restartArgv;
static int              restartArgc;

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
        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
        [[NSNotificationCenter defaultCenter] addObserver:app
            selector:@selector(orientationChanged:)
            name:UIDeviceOrientationDidChangeNotification
            object:[UIDevice currentDevice]];
        
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
    
    app = self;
    
    /* Start pjsua app thread */
    [NSThread detachNewThreadSelector:@selector(pjsuaStart) toTarget:self withObject:nil];

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
    
    NSLog(@"Device orientation changed: %d", (prev_ori = dev_ori));
    
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
    
    /* Since iOS requires that the minimum keep alive interval is 600s,
     * application needs to make sure that the account's registration
     * timeout is long enough.
     */
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
    [application setKeepAliveTimeout:KEEP_ALIVE_INTERVAL handler: ^{
	[self performSelectorOnMainThread:@selector(keepAlive) withObject:nil waitUntilDone:YES];
    }];
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
	
	[[UIApplication sharedApplication] presentLocalNotificationNow:alert];
    }
    
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
        
        if (pjsua_vid_win_get_info(i, &wi) == PJ_SUCCESS) {
            UIView *parent = app.viewController.view;
            UIView *view = (__bridge UIView *)wi.hwnd.info.ios.window;
            
            if (view) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    /* Add the video window as subview */
                    if (![view isDescendantOfView:parent])
                        [parent addSubview:view];
                    
                    if (!wi.is_native) {
                        /* Resize it to fit width */
                        view.bounds = CGRectMake(0, 0, parent.bounds.size.width,
                                                 (parent.bounds.size.height *
                                                  1.0*parent.bounds.size.width/
                                                  view.bounds.size.width));
                        /* Center it horizontally */
                        view.center = CGPointMake(parent.bounds.size.width/2.0,
                                              view.bounds.size.height/2.0);
                    } else {
                        /* Preview window, move it to the bottom */
                        view.center = CGPointMake(parent.bounds.size.width/2.0,
                                                  parent.bounds.size.height-
                                                  view.bounds.size.height/2.0);
                    }
                });
            }
        }
    }

    
#endif
}

@end
