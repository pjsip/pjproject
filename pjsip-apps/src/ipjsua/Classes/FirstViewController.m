//
//  FirstViewController.m
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/23/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

#import "FirstViewController.h"
#import "ipjsuaAppDelegate.h"


@implementation FirstViewController
@synthesize textField;
@synthesize textView;
@synthesize button1;
@synthesize text;
@synthesize hasInput;

- (BOOL)textFieldShouldReturn:(UITextField *)theTextField {
    // When the user presses return, take focus away from the text field so that the keyboard is dismissed.
    if (theTextField == textField) {
	[self.textField resignFirstResponder];
	self.hasInput = true;
        self.text = [textField.text stringByAppendingString:@"\n"];
	textField.text = @"";
    }
    return YES;
}


- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    // Dismiss the keyboard when the view outside the text field is touched.
    [textField resignFirstResponder];
    [super touchesBegan:touches withEvent:event];
}

/*
// The designated initializer. Override to perform setup that is required before the view is loaded.
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
    
    [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
    
    ipjsuaAppDelegate *appd = (ipjsuaAppDelegate *)[[UIApplication sharedApplication] delegate];
    appd.mainView = self;
    textField.delegate = self;
    [self.textView setFont:[UIFont fontWithName:@"Courier New" size:8.9]];
    [self.textField setEnabled: false];
    [button1 addTarget:self action:@selector(button1Pressed:) forControlEvents:(UIControlEvents)UIControlEventTouchDown];
}

- (void)button1Pressed:(id)sender {
    /* Clear the text view */
    self.textView.text = @"";
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationLandscapeRight);
}

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
