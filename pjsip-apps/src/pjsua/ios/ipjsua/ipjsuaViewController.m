//
//  ipjsuaViewController.m
//  ipjsua
//
//  Created by Liong Sauw Ming on 13/3/13.
//  Copyright (c) 2013 Teluu. All rights reserved.
//

#import "ipjsuaViewController.h"

@interface ipjsuaViewController ()

@end

@implementation ipjsuaViewController

@synthesize textLabel;

- (void)viewDidLoad
{
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.

    [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
