//
//  ipjsystestAppDelegate.h
//  ipjsystest
//
//  Created by Liong Sauw Ming on 3/20/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

@interface ipjsystestAppDelegate : NSObject <UIApplicationDelegate> {
    
    UIWindow		    *window;
    UINavigationController  *navigationController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet UINavigationController *navigationController;

@end

