//
//  RootViewController.h
//  ipjsystest
//
//  Created by Liong Sauw Ming on 3/20/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

#import "TestViewController.h"

@interface RootViewController : UITableViewController {
    NSMutableArray *titles;
    NSMutableArray *menus;
    
    TestViewController *testView;
}

@property (nonatomic, retain) NSMutableArray *titles;
@property (nonatomic, retain) NSMutableArray *menus;
@property (nonatomic, retain) TestViewController *testView;

@end
