//
//  SoundController.h
//  CoreAudioPlayer
//
//  Created by James on 02/09/2016.
//  Copyright © 2016 James Alvarez. All rights reserved.
//

#ifndef SoundController_h
#define SoundController_h

#include <stdio.h>
#include <AudioToolbox/AudioToolbox.h>


// A struct to hold data and playback status

typedef struct CAPAudioPlayer {
    AudioBufferList *bufferList;
    UInt32 frames;
    UInt32 currentFrame;
} CAPAudioPlayer;


// A struct to hold information about output status

typedef struct CAPAudioOutput
{
    AudioUnit outputUnit;
    double startingFrameCount;
    CAPAudioPlayer player;
} CAPAudioOutput;


void CAPDisposeAudioPlayer(CAPAudioPlayer * audioPlayer);
void CAPLoadAudioPlayer(CFURLRef url, CAPAudioPlayer * audioPlayer);

void CAPStartAudioOutput (CAPAudioOutput *player);
void CAPDisposeAudioOutput(CAPAudioOutput *output);

#endif /* SoundController_h */
