/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
(Borrowed from aurioTouch sample code) This class demonstrates the audio APIs used to capture audio data from the microphone and play it out to the speaker. It also demonstrates how to play system sounds.
*/

#import <AVFoundation/AVFoundation.h>

@interface AudioController : NSObject

@property (nonatomic, assign) BOOL muteAudio;
@property (nonatomic, assign, readonly) BOOL audioChainIsBeingReconstructed;

- (BOOL)startIOUnit;
- (void)stopIOUnit;

@end
