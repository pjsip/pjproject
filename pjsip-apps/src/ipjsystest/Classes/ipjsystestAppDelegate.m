//
//  ipjsystestAppDelegate.m
//  ipjsystest
//
//  Created by Liong Sauw Ming on 3/20/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

#import "ipjsystestAppDelegate.h"
#import "RootViewController.h"


@implementation ipjsystestAppDelegate

@synthesize window;
@synthesize navigationController;

#pragma mark -
#pragma mark Application lifecycle

- (void)applicationDidFinishLaunching:(UIApplication *)application {    
    
    // Override point for customization after app launch
    [window addSubview:[navigationController view]];
    [window makeKeyAndVisible];
}


- (void)applicationWillTerminate:(UIApplication *)application {
    // Save data if appropriate
}


#pragma mark -
#pragma mark Memory management

- (void)dealloc {
    [navigationController release];
    [window release];
    [super dealloc];
}


@end

