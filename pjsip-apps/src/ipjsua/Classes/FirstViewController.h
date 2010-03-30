//
//  FirstViewController.h
//  ipjsua
//
//  Created by Liong Sauw Ming on 3/23/10.
//  Copyright Teluu Inc. (http://www.teluu.com) 2010. All rights reserved.
//

#import <UIKit/UIKit.h>


@interface FirstViewController : UIViewController<UITextFieldDelegate> {
    IBOutlet UITextField *textField;
    IBOutlet UITextView  *textView;
    IBOutlet UIButton	 *button1;

    NSString		 *text;
    bool		 hasInput;
}

@property (nonatomic, retain) IBOutlet UITextField *textField;
@property (nonatomic, retain) IBOutlet UITextView *textView;
@property (nonatomic, retain) IBOutlet UIButton *button1;
@property (nonatomic, retain) NSString *text;
@property (nonatomic) bool hasInput;

@end
