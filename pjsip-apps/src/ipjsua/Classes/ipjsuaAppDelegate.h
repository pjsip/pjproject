//
//  ipjsuaAppDelegate.h
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/23/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ConfigViewController.h"
#import "FirstViewController.h"
#import "TabBarController.h"

@interface ipjsuaAppDelegate : NSObject <UIApplicationDelegate, UITabBarControllerDelegate> {
    UIWindow		 *window;
    ConfigViewController *cfgView;
    FirstViewController  *mainView;
    TabBarController	 *tabBarController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet TabBarController *tabBarController;
@property (nonatomic, retain) IBOutlet ConfigViewController *cfgView;
@property (nonatomic, retain) FirstViewController *mainView;


@end
