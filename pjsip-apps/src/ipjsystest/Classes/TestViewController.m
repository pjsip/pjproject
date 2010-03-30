//
//  TestViewController.m
//  ipjsystest
//
//  Created by Liong Sauw Ming on 3/20/10.
//  Copyright 2010 Teluu Inc. (http://www.teluu.com). All rights reserved.
//

#import "TestViewController.h"

@implementation TestViewController
@synthesize testDesc;
@synthesize button1;
@synthesize button2;
@synthesize key;

- (void)button1Pressed:(id)sender {
    self.key = 1;
    [self.button1 setHidden:true];
    [self.button2 setHidden:true];
}

- (void)button2Pressed:(id)sender {
    self.key = 2;
    [self.button1 setHidden:true];
    [self.button2 setHidden:true];
}

/*
 // The designated initializer.  Override if you create the controller programmatically and want to perform customization that is not appropriate for viewDidLoad.
 - (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]) {
    // Custom initialization
 }
 return self;
 }
 */

/*
 // Implement loadView to create a view hierarchy programmatically, without using a nib.
 - (void)loadView {
 }
 */


// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad {
    [super viewDidLoad];
    
    [button1 addTarget:self action:@selector(button1Pressed:) forControlEvents:(UIControlEvents)UIControlEventTouchDown];
    [button2 addTarget:self action:@selector(button2Pressed:) forControlEvents:(UIControlEvents)UIControlEventTouchDown];
    [testDesc setEditable:false];
}

/*
 // Override to allow orientations other than the default portrait orientation.
 - (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
 }
 */

- (void)didReceiveMemoryWarning {
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}


- (void)dealloc {
    [super dealloc];
}


@end
