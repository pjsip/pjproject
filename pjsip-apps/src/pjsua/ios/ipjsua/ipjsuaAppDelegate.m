//
//  ipjsuaAppDelegate.m
//  ipjsua
//
//  Created by Liong Sauw Ming on 13/3/13.
//  Copyright (c) 2013 Teluu. All rights reserved.
//

#import "ipjsuaAppDelegate.h"
#import <pjlib.h>
#import <pjsua.h>
#import <pj/log.h>
#include "../../pjsua_common.h"

#import "ipjsuaViewController.h"

@implementation ipjsuaAppDelegate

#define KEEP_ALIVE_INTERVAL 600

static int _argc = 4;
static char *_argv[] = {"",
    "--use-cli",
    "--no-cli-console",
    "--cli-telnet-port=6378"
};

ipjsuaAppDelegate              *app;
extern pj_cli_telnet_on_started on_started_cb;
extern pj_cli_on_quit           on_quit_cb;
extern pj_cli_on_destroy        on_destroy_cb;
extern pj_cli_on_restart_pjsua  on_restart_pjsua_cb;
static pj_thread_desc           a_thread_desc;
static pj_thread_t             *a_thread;

int main_func(int argc, char *argv[]);

static void cli_telnet_started(pj_cli_telnet_info *telnet_info)
{
    PJ_LOG(3,("ipjsua", "Telnet to %.*s:%d",
	      telnet_info->ip_address.slen, telnet_info->ip_address.ptr,
	      telnet_info->port));
    NSString *str = [NSString stringWithFormat:@"Telnet to %.*s:%d",
                     (int)telnet_info->ip_address.slen,
                     telnet_info->ip_address.ptr,
                     telnet_info->port];
    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:str
         waitUntilDone:NO];
}

static void cli_on_quit (pj_bool_t is_restarted)
{
    PJ_LOG(3,("ipjsua", "CLI quit, restart(%d)", is_restarted));
    if (!is_restarted) {
        NSString *str = [NSString stringWithFormat:@"CLI quit, "
                         "telnet unavailable"];
        [app performSelectorOnMainThread:@selector(displayMsg:) withObject:str
             waitUntilDone:NO];
    }
}

- (void)displayMsg:(NSString *)str {
    app.viewController.textLabel.text = str;
}

- (void)start_app {
    on_started_cb = &cli_telnet_started;
    on_quit_cb = &cli_on_quit;
    main_func(_argc, _argv);
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
    [NSThread detachNewThreadSelector:@selector(start_app) toTarget:self withObject:nil];

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)keepAlive {
    int i, timeout = KEEP_ALIVE_INTERVAL;
    
    if (!pj_thread_is_registered())
    {
	pj_thread_register("ipjsua", a_thread_desc, &a_thread);
    }
    
    for (i = 0; i < (int)pjsua_acc_get_count(); ++i) {
        if (pjsua_acc_is_valid(i)) {
            pjsua_acc_config acc_cfg;

	    pjsua_acc_get_config(i, &acc_cfg);
            if (!acc_cfg.reg_uri.slen)
                continue;
            if (acc_cfg.reg_timeout < timeout) {
                acc_cfg.reg_timeout = timeout;
                pjsua_acc_modify(i, &acc_cfg);
            } else {
                pjsua_acc_set_registration(i, PJ_TRUE);
            }
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

@end
