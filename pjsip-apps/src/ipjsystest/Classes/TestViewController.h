//
//  TestViewController.h
//  ipjsystest
//
//  Created by Liong Sauw Ming on 3/20/10.
//  Copyright 2010 Teluu Inc. (http://www.teluu.com). All rights reserved.
//

#import <UIKit/UIKit.h>


@interface TestViewController : UIViewController {
    IBOutlet UITextView *testDesc;
    IBOutlet UIButton	*button1;
    IBOutlet UIButton	*button2;
    
    NSInteger key;
}

@property (nonatomic, retain) IBOutlet UITextView *testDesc;
@property (nonatomic, retain) IBOutlet UIButton *button1;
@property (nonatomic, retain) IBOutlet UIButton *button2;
@property (nonatomic) NSInteger key;

@end
