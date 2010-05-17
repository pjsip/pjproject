//
//  ConfigViewController.m
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/25/10.
//  Copyright 2010 Teluu Inc. (http://www.teluu.com). All rights reserved.
//

#import "ConfigViewController.h"
#import "ipjsuaAppDelegate.h"


@implementation ConfigViewController
@synthesize textView;
@synthesize button1;
@synthesize button2;

bool kshow = false;

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

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    // Dismiss the keyboard when the view outside the text view is touched.
    [textView resignFirstResponder];
    [super touchesBegan:touches withEvent:event];
}

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad {
    [super viewDidLoad];
    
    [textView setFont:[UIFont fontWithName:@"Courier" size:10]];
    ipjsuaAppDelegate *appd = (ipjsuaAppDelegate *)[[UIApplication sharedApplication] delegate];
    appd.cfgView = self;
    
    /* Load config file and display it in the text view */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *cfgPath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"/config.cfg"];
    textView.text = [NSMutableString stringWithContentsOfFile:cfgPath encoding:NSASCIIStringEncoding error:NULL];
    
    /* Add keyboard show/hide notifications so that we can resize the text view */
    kshow = false;
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(keyboardWillShow:) name: UIKeyboardWillShowNotification object:nil];
    [nc addObserver:self selector:@selector(keyboardWillHide:) name: UIKeyboardWillHideNotification object:nil];
    
    /* Add button press event-handlers */
    [self.button1 addTarget:self action:@selector(button1Pressed:) forControlEvents:(UIControlEvents)UIControlEventTouchDown];
    [self.button2 addTarget:self action:@selector(button2Pressed:) forControlEvents:(UIControlEvents)UIControlEventTouchDown];
}

-(void) keyboardWillShow:(NSNotification *) note
{
    if (kshow) return;
    
    /* Shrink the text view area when the keyboard appears */
    [UIView beginAnimations:nil context:NULL];
    [UIView setAnimationDuration:0.3];
    CGRect r  = self.textView.frame, t;
    [[note.userInfo valueForKey:UIKeyboardBoundsUserInfoKey] getValue: &t];
    r.size.height -=  t.size.height - 51;
    self.textView.frame = r;
    [UIView commitAnimations];
    kshow = true;
    
    [self.button1 setEnabled:true];
    [self.button1.titleLabel setEnabled:true];
    [self.button2 setEnabled:true];
    [self.button2.titleLabel setEnabled:true];
}

-(void) keyboardWillHide:(NSNotification *) note
{
    CGRect r  = self.textView.frame, t;
    [[note.userInfo valueForKey:UIKeyboardBoundsUserInfoKey] getValue: &t];
    r.size.height +=  t.size.height - 51;
    self.textView.frame = r;
    kshow = false;
}

- (void)button1Pressed:(id)sender {
    /* Save the config file */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *cfgPath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"/config.cfg"];
    [self.textView.text writeToFile:cfgPath atomically:NO encoding:NSASCIIStringEncoding error:NULL];
    
    [self.textView resignFirstResponder];
    [self.button1 setEnabled:false];
    [self.button1.titleLabel setEnabled:false];
    [self.button2 setEnabled:false];
    [self.button2.titleLabel setEnabled:false];
}

- (void)button2Pressed:(id)sender {
    /* Reload the config file */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *cfgPath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"/config.cfg"];
    self.textView.text = [NSMutableString stringWithContentsOfFile:cfgPath encoding:NSASCIIStringEncoding error:NULL];
    
    [self.textView resignFirstResponder];
    [self.button1 setEnabled:false];
    [self.button1.titleLabel setEnabled:false];
    [self.button2 setEnabled:false];
    [self.button2.titleLabel setEnabled:false];
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
