//
//  AppDelegate.m
//  CoreAudioPlayer
//
//  Created by James on 08/09/2016.
//  Copyright © 2016 James Alvarez. All rights reserved.
//

#import "AppDelegate.h"
#import <AVFoundation/AVFoundation.h>
#import "SoundController.h"

@interface AppDelegate ()

@end

@implementation AppDelegate {
    CAPAudioOutput _audioOutput;
}


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Set up AVAudioSession to allow audio on iOS device
    NSError *error = nil;
    if ( ![[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:&error] ) {
        NSLog(@"Couldn't set audio session category: %@", error);
    }
    
    if ( ![[AVAudioSession sharedInstance] setPreferredIOBufferDuration:(128.0/44100.0) error:&error] ) {
        NSLog(@"Couldn't set preferred buffer duration: %@", error);
    }
    
    if ( ![[AVAudioSession sharedInstance] setActive:YES error:&error] ) {
        NSLog(@"Couldn't set audio session active: %@", error);
    }
    
    NSURL* url = [[NSBundle mainBundle] URLForResource:@"Are You Ready" withExtension:@"mp3"];
    
    
    
    // Load audio file
    CAPLoadAudioPlayer((__bridge CFURLRef) url, &_audioOutput.player);
    
    
    // Start audio
    CAPStartAudioOutput(&_audioOutput);
    
    return YES;
}

-(void)dealloc {
    CAPDisposeAudioOutput(&_audioOutput);
    CAPDisposeAudioPlayer(&_audioOutput.player);
}


@end
