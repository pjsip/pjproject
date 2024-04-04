//
//  ipjsuaAppDelegate.h
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
#import <CallKit/CallKit.h>
#import <PushKit/PushKit.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

@class ipjsuaViewController;

@interface ipjsuaAppDelegate : UIResponder
    <UIApplicationDelegate, PKPushRegistryDelegate,
     UNUserNotificationCenterDelegate, CXProviderDelegate>

@property (strong, nonatomic) UIWindow *window;

@property (strong, nonatomic) PKPushRegistry *voipRegistry;
@property (strong, nonatomic) NSMutableString *token;
@property (strong, nonatomic) CXProvider *provider;

@property (strong, nonatomic) ipjsuaViewController *viewController;

@end
