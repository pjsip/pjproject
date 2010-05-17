//
//  TabBarController.m
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/24/10.
//  Copyright 2010 Teluu Inc. (http://www.teluu.com). All rights reserved.
//

#import "TabBarController.h"


@implementation TabBarController

// Override to allow orientations other than the default portrait orientation.
- (BOOL) shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationLandscapeRight);
}

@end
