//
//  ConfigViewController.h
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/25/10.
//  Copyright 2010 Teluu Inc. (http://www.teluu.com). All rights reserved.
//

#import <UIKit/UIKit.h>


@interface ConfigViewController : UIViewController {
    IBOutlet UITextView *textView;
    IBOutlet UIButton	*button1;
    IBOutlet UIButton	*button2;
}

@property (nonatomic, retain) IBOutlet UITextView *textView;
@property (nonatomic, retain) IBOutlet UIButton *button1;
@property (nonatomic, retain) IBOutlet UIButton *button2;

@end
