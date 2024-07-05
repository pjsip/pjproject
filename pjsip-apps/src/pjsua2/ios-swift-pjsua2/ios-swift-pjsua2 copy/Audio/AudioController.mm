/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
The logic for VOIP call audio sessions.
*/

#import "AudioController.h"

// Framework includes
#import <AVFoundation/AVAudioSession.h>

// Utility file includes
#import "CAXException.h"
#import "CAStreamBasicDescription.h"

@interface AudioController () {
    AVAudioEngine *_engine;
    BOOL _audioChainIsBeingReconstructed;
}

- (void)setupAudioSession;
- (void)setupIOUnit;
- (void)setupAudioChain;

@end

@implementation AudioController

@synthesize muteAudio = _muteAudio;

- (id)init
{
    if (self = [super init]) {
        [self setupAudioChain];
    }
    return self;
}


- (void)handleInterruption:(NSNotification *)notification
{
    try {
        UInt8 theInterruptionType = [[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] intValue];
        NSLog(@"Session interrupted > --- %s ---\n", theInterruptionType == AVAudioSessionInterruptionTypeBegan ? "Begin Interruption" : "End Interruption");

        if (theInterruptionType == AVAudioSessionInterruptionTypeBegan) {
            [self stopIOUnit];
        }

        if (theInterruptionType == AVAudioSessionInterruptionTypeEnded) {
            // Make sure to activate the session.
            NSError *error = nil;
            [[AVAudioSession sharedInstance] setActive:YES error:&error];
            if (nil != error) NSLog(@"AVAudioSession set active failed with error: %@", error);

            [self startIOUnit];
        }
    } catch (CAXException e) {
        char buf[256];
        fprintf(stderr, "Error: %s (%s)\n", e.mOperation, e.FormatError(buf));
    }
}

- (void)handleRouteChange:(NSNotification *)notification
{
    UInt8 reasonValue = [[notification.userInfo valueForKey:AVAudioSessionRouteChangeReasonKey] intValue];
    AVAudioSessionRouteDescription *routeDescription = [notification.userInfo valueForKey:AVAudioSessionRouteChangePreviousRouteKey];

    NSLog(@"Route change:");
    switch (reasonValue) {
        case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
            NSLog(@"     NewDeviceAvailable");
            break;
        case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
            NSLog(@"     OldDeviceUnavailable");
            break;
        case AVAudioSessionRouteChangeReasonCategoryChange:
            NSLog(@"     CategoryChange");
            NSLog(@" New Category: %@", [[AVAudioSession sharedInstance] category]);
            break;
        case AVAudioSessionRouteChangeReasonOverride:
            NSLog(@"     Override");
            break;
        case AVAudioSessionRouteChangeReasonWakeFromSleep:
            NSLog(@"     WakeFromSleep");
            break;
        case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
            NSLog(@"     NoSuitableRouteForCategory");
            break;
        default:
            NSLog(@"     ReasonUnknown");
    }

    NSLog(@"Previous route:\n");
    NSLog(@"%@", routeDescription);
}

- (void)handleMediaServerReset:(NSNotification *)notification
{
    NSLog(@"Media server has reset");
    _audioChainIsBeingReconstructed = YES;

    // Wait here for some time to ensure objects are not deleted while they're being accessed elsewhere.
    usleep(25000);

    // Rebuild the audio chain.
    [self setupAudioChain];
    [self startIOUnit];

    _audioChainIsBeingReconstructed = NO;
}

- (void)setupAudioSession
{
    try {
        // Configure the audio session.
        AVAudioSession *sessionInstance = [AVAudioSession sharedInstance];

        // Pick the play and record category.
        NSError *error = nil;
        [sessionInstance setCategory:AVAudioSessionCategoryPlayAndRecord error:&error];
        XThrowIfError((OSStatus)error.code, "Couldn't set session's audio category");

        // Set the mode to voice chat.
        [sessionInstance setMode:AVAudioSessionModeVoiceChat error:&error];
        XThrowIfError((OSStatus)error.code, "Couldn't set session's audio mode");

#if TARGET_OS_IOS
        // Set the buffer duration to 5 ms.
        NSTimeInterval bufferDuration = .005;
        [sessionInstance setPreferredIOBufferDuration:bufferDuration error:&error];
        XThrowIfError((OSStatus)error.code, "Couldn't set session's I/O buffer duration");

        // Set the session's sample rate.
        [sessionInstance setPreferredSampleRate:44100 error:&error];
        XThrowIfError((OSStatus)error.code, "Couldn't set session's preferred sample rate");
#endif

        // Add interruption handler.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleInterruption:)
                                                     name:AVAudioSessionInterruptionNotification
                                                   object:sessionInstance];

        // Add the route change notification.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleRouteChange:)
                                                     name:AVAudioSessionRouteChangeNotification
                                                   object:sessionInstance];

        // Rebuild the audio chain if media services are reset.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleMediaServerReset:)
                                                     name:AVAudioSessionMediaServicesWereResetNotification
                                                   object:sessionInstance];
    }

    catch (CAXException &e) {
        NSLog(@"Error returned from setupAudioSession: %d: %s", (int)e.mError, e.mOperation);
    }
    catch (...) {
        NSLog(@"Unknown error returned from setupAudioSession");
    }

    return;
}

- (void)setupIOUnit
{
    AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:1];
    NSError *error;
    _engine = [[AVAudioEngine alloc] init];
    [_engine.inputNode setVoiceProcessingEnabled:YES error:&error];
    [_engine connect:_engine.inputNode to:_engine.outputNode format:format];
    [_engine prepare];

}

- (void)setupAudioChain
{
    [self setupAudioSession];
    [self setupIOUnit];
}

- (BOOL)startIOUnit
{
    NSError *error;
    [_engine startAndReturnError:&error];
    if (error) NSLog(@"Couldn't start Apple Voice Processing IO: %@", error);
    return NO;
}

- (void)stopIOUnit
{
    [_engine stop];
}

- (BOOL)audioChainIsBeingReconstructed
{
    return _audioChainIsBeingReconstructed;
}

@end
