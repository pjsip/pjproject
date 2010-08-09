//
//  ipjsuaAppDelegate.m
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/23/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

#import <pjlib.h>
#import "ipjsuaAppDelegate.h"

extern pj_log_func *log_cb;

@implementation ipjsuaAppDelegate
@synthesize window;
@synthesize tabBarController;
@synthesize mainView;
@synthesize cfgView;

/* Sleep interval duration */
#define SLEEP_INTERVAL	0.5
/* Determine whether we should print the messages in the debugger
 * console as well
 */
#define DEBUGGER_PRINT	1
/* Whether we should show pj log messages in the text area */
#define SHOW_LOG	1
#define PATH_LENGTH	PJ_MAXPATH

extern pj_bool_t app_restart;

char argv_buf[PATH_LENGTH];
char *argv[] = {"", "--config-file", argv_buf};

ipjsuaAppDelegate	*app;

bool			app_running;
bool			thread_quit;
NSMutableString		*mstr;

pj_status_t app_init(int argc, char *argv[]);
pj_status_t app_main(void);
pj_status_t app_destroy(void);

void showMsg(const char *format, ...)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    va_list arg;
    
    va_start(arg, format);
    NSString *str = [[NSString alloc] initWithFormat:[NSString stringWithFormat:@"%s", format] arguments: arg];
#if DEBUGGER_PRINT
    NSLog(str);
#endif
    va_end(arg);
    
    [mstr appendString:str];
    [pool release];
}

char * getInput(char *s, int n, FILE *stream)
{
    if (stream != stdin) {
	return fgets(s, n, stream);
    }
    
    app.mainView.hasInput = false;
    [app.mainView.textField setEnabled: true];
    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:mstr waitUntilDone:YES];
    [mstr setString:@""];
    
    while (!thread_quit && !app.mainView.hasInput) {
	int ctr = 0;
	[NSThread sleepForTimeInterval:SLEEP_INTERVAL];
	if (ctr == 4) {
	    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:mstr waitUntilDone:YES];
	    [mstr setString:@""];
	    ctr = 0;
	}
	ctr++;
    }
    
    [app.mainView.text getCString:s maxLength:n encoding:NSASCIIStringEncoding];
    [app.mainView.textField setEnabled: false];    
    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:app.mainView.text waitUntilDone:NO];
    
    return s;
}

void showLog(int level, const char *data, int len)
{
    showMsg("%s", data);
}

- (void)start_app {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    /* Wait until the view is ready */
    while (self.mainView == nil) {
	[NSThread sleepForTimeInterval:SLEEP_INTERVAL];
    }    

    [NSThread setThreadPriority:1.0];
    mstr = [NSMutableString stringWithCapacity:4196];
#if SHOW_LOG
    pj_log_set_log_func(&showLog);
    log_cb = &showLog;
#endif
    
    do {
	app_restart = PJ_FALSE;
	if (app_init(3, argv) != PJ_SUCCESS) {
	    NSString *str = @"Failed to initialize pjsua\n";
	    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:str waitUntilDone:YES];
	} else {
	    app_running = true;
	    app_main();
	    
	    app_destroy();
	    /* This is on purpose */
	    app_destroy();
	}

	[app performSelectorOnMainThread:@selector(displayMsg:) withObject:mstr waitUntilDone:YES];
	[mstr setString:@""];
    } while (app_restart);
    
    [pool release];
}

- (void)displayMsg:(NSString *)str {
    self.mainView.textView.text = [self.mainView.textView.text stringByAppendingString:str];
    [self.mainView.textView scrollRangeToVisible:NSMakeRange([self.mainView.textView.text length] - 1, 1)];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
    /* If there is no config file in the document dir, copy the default config file into the directory */ 
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *cfgPath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"/config.cfg"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:cfgPath]) {
	NSString *resPath = [[NSBundle mainBundle] pathForResource:@"config" ofType:@"cfg"];
	NSString *cfg = [NSString stringWithContentsOfFile:resPath encoding:NSASCIIStringEncoding error:NULL];
	[cfg writeToFile:cfgPath atomically:NO encoding:NSASCIIStringEncoding error:NULL];
    }
    [cfgPath getCString:argv[2] maxLength:PATH_LENGTH encoding:NSASCIIStringEncoding];    
    
    // Add the tab bar controller's current view as a subview of the window
    [window addSubview:tabBarController.view];
    [window makeKeyAndVisible];

    app = self;
    app_running = false;
    thread_quit = false;
    /* Start pjsua thread */
    [NSThread detachNewThreadSelector:@selector(start_app) toTarget:self withObject:nil];
}

/*
// Optional UITabBarControllerDelegate method
- (void)tabBarController:(UITabBarController *)tabBarController didSelectViewController:(UIViewController *)viewController {
}
*/

/*
// Optional UITabBarControllerDelegate method
- (void)tabBarController:(UITabBarController *)tabBarController didEndCustomizingViewControllers:(NSArray *)viewControllers changed:(BOOL)changed {
}
*/


- (void)dealloc {
    thread_quit = true;
    [NSThread sleepForTimeInterval:SLEEP_INTERVAL];
    
    [tabBarController release];
    [window release];
    [super dealloc];
}

@end

