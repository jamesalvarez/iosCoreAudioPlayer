//
//  SoundController.c
//  CoreAudioPlayer
//
//  Created by James on 02/09/2016.
//  Copyright © 2016 James Alvarez. All rights reserved.
//


#include "SoundController.h"


#define CAP_SAMPLE_RATE 44100
#define CAP_CHANNELS 2
#define CAP_SAMPLE_SIZE sizeof(Float32)


AudioStreamBasicDescription const CAPAudioDescription = {
    .mSampleRate        = CAP_SAMPLE_RATE,
    .mFormatID          = kAudioFormatLinearPCM,
    .mFormatFlags       = kAudioFormatFlagIsFloat,
    .mBytesPerPacket    = CAP_SAMPLE_SIZE * CAP_CHANNELS,
    .mFramesPerPacket   = 1,
    .mBytesPerFrame     = CAP_CHANNELS * CAP_SAMPLE_SIZE,
    .mChannelsPerFrame  = CAP_CHANNELS,
    .mBitsPerChannel    = 8 * CAP_SAMPLE_SIZE, //8 bits per byte
    .mReserved          = 0
};


#pragma mark - callback function -

static OSStatus CAPRenderProc(void *inRefCon,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList * ioData) {
    
    CAPAudioOutput *audioOutput = (CAPAudioOutput*)inRefCon;
    CAPAudioPlayer *audioPlayer = &audioOutput->player;
    
    UInt32 currentFrame = audioPlayer->currentFrame;
    UInt32 maxFrames = audioPlayer->frames;
    
    Float32 *outputData = (Float32*)ioData->mBuffers[0].mData;
    Float32 *inputData = (Float32*)audioPlayer->bufferList->mBuffers[0].mData;
    
    for (UInt32 frame = 0; frame < inNumberFrames; ++frame) {
        UInt32 outSample = frame * 2;
        UInt32 inSample = currentFrame * 2;
        
        (outputData)[outSample] = (inputData)[inSample];
        (outputData)[outSample+1] = (inputData)[inSample + 1];
        
        currentFrame++;
        currentFrame = currentFrame % maxFrames; // loop
    }
    
    audioPlayer->currentFrame = currentFrame;
    
    return noErr;
}

#pragma mark - error handler -

// generic error handler - if err is nonzero, prints error message and exits program.
static void CheckError(OSStatus error, const char *operation) {
    if (error == noErr) return;
    
    char str[20];
    // see if it appears to be a 4-char-code
    *(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error);
    if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
        str[0] = str[5] = '\'';
        str[6] = '\0';
    } else
        // no, format it as an integer
        sprintf(str, "%d", (int)error);
    
    fprintf(stderr, "Error: %s (%s)\n", operation, str);
    
    exit(1);
}

#pragma mark - audio player functions -

void CAPLoadAudioPlayer(CFURLRef url, CAPAudioPlayer * audioPlayer) {
    ExtAudioFileRef audioFile;
    OSStatus status = noErr;
    
    
    // Open file
    status = ExtAudioFileOpenURL(url, &audioFile);
    CheckError(status,"Could not open audio file");
    
    
    // Get files information
    AudioStreamBasicDescription fileAudioDescription;
    UInt32 size = sizeof(fileAudioDescription);
    status = ExtAudioFileGetProperty(audioFile,
                                     kExtAudioFileProperty_FileDataFormat,
                                     &size,
                                     &fileAudioDescription);
    CheckError(status,"Could not get file data format");
    
    
    // Apply audio format
    status = ExtAudioFileSetProperty(audioFile,
                                     kExtAudioFileProperty_ClientDataFormat,
                                     sizeof(CAPAudioDescription),
                                     &CAPAudioDescription);
    CheckError(status,"Could not apply audio format");
    
    
    // Determine length in frames (in original file's sample rate)
    UInt64 fileLengthInFrames;
    size = sizeof(fileLengthInFrames);
    status = ExtAudioFileGetProperty(audioFile,
                                     kExtAudioFileProperty_FileLengthFrames,
                                     &size,
                                     &fileLengthInFrames);
    CheckError(status,"Could not get file length in frames");
    
    
    // Calculate the true length in frames, given the original and target sample rates
    fileLengthInFrames = ceil(fileLengthInFrames * (CAPAudioDescription.mSampleRate / fileAudioDescription.mSampleRate));
    
    
    // Prepare AudioBufferList: Interleaved data uses just one buffer, non-interleaved has two
    int numberOfBuffers = CAPAudioDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved ? CAPAudioDescription.mChannelsPerFrame : 1;
    int channelsPerBuffer = CAPAudioDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved ? 1 : CAPAudioDescription.mChannelsPerFrame;
    int bytesPerBuffer = CAPAudioDescription.mBytesPerFrame * (int)fileLengthInFrames;
    
    AudioBufferList *bufferList = malloc(sizeof(AudioBufferList) + (numberOfBuffers-1)*sizeof(AudioBuffer));
    
    if ( !bufferList ) {
        ExtAudioFileDispose(audioFile);
        printf("Could not allocate memory for bufferList");
        exit(1);
        return;
    }
    
    bufferList->mNumberBuffers = numberOfBuffers;
    for ( int i=0; i<numberOfBuffers; i++ ) {
        if ( bytesPerBuffer > 0 ) {
            bufferList->mBuffers[i].mData = calloc(bytesPerBuffer, 1);
            if ( !bufferList->mBuffers[i].mData ) {
                for ( int j=0; j<i; j++ ) {
                    free(bufferList->mBuffers[j].mData);
                }
                free(bufferList);
                ExtAudioFileDispose(audioFile);
                printf("Could not allocate memory for buffer");
                exit(1);
                return;
            }
        } else {
            bufferList->mBuffers[i].mData = NULL;
        }
        bufferList->mBuffers[i].mDataByteSize = bytesPerBuffer;
        bufferList->mBuffers[i].mNumberChannels = channelsPerBuffer;
    }
    
    
    
    
    // Create a stack copy of the given audio buffer list and offset mData pointers, with offset in bytes
    char scratchBufferList_bytes[sizeof(AudioBufferList)+(sizeof(AudioBuffer)*(bufferList->mNumberBuffers-1))];
    memcpy(scratchBufferList_bytes, bufferList, sizeof(scratchBufferList_bytes));
    AudioBufferList * scratchBufferList = (AudioBufferList*)scratchBufferList_bytes;
    for ( int i=0; i<scratchBufferList->mNumberBuffers; i++ ) {
        scratchBufferList->mBuffers[i].mData = (char*)scratchBufferList->mBuffers[i].mData;
    }
    
    // Perform read in multiple small chunks (otherwise ExtAudioFileRead crashes when performing sample rate conversion)
    UInt32 readFrames = 0;
    while (readFrames < fileLengthInFrames) {
        UInt32 framesLeftToRead = (UInt32)fileLengthInFrames - readFrames;
        UInt32 framesToRead = (framesLeftToRead < 16384) ? framesLeftToRead : 16384;
        
        // Set the scratch buffer to point to the correct position on the real buffer
        scratchBufferList->mNumberBuffers = bufferList->mNumberBuffers;
        for ( int i=0; i<bufferList->mNumberBuffers; i++ ) {
            scratchBufferList->mBuffers[i].mNumberChannels = bufferList->mBuffers[i].mNumberChannels;
            scratchBufferList->mBuffers[i].mData = bufferList->mBuffers[i].mData + (readFrames * CAPAudioDescription.mBytesPerFrame);
            scratchBufferList->mBuffers[i].mDataByteSize = framesToRead * CAPAudioDescription.mBytesPerFrame;
        }
        
        // Perform read
        status = ExtAudioFileRead(audioFile, &framesToRead, scratchBufferList);
        
        // Break if no frames were read
        if ( framesToRead == 0 ) break;
        
        readFrames += framesToRead;
    }
    
    
    // Clean up
    ExtAudioFileDispose(audioFile);
    
    
    // BufferList and readFrames are the audio we loaded
    audioPlayer->bufferList = bufferList;
    audioPlayer->frames = readFrames;
}

void CAPDisposeAudioPlayer(CAPAudioPlayer * audioPlayer) {
    if (audioPlayer != NULL) {
        AudioBufferList * bufferList = audioPlayer->bufferList;
        if (bufferList != NULL) {
            for ( int j=0; j<bufferList->mNumberBuffers; j++ ) {
                free(bufferList->mBuffers[j].mData);
            }
            free(bufferList);
        }
    }
}

#pragma mark - audio output functions -

void CAPStartAudioOutput (CAPAudioOutput *player) {
    OSStatus status = noErr;
    
    // Description for the output AudioComponent
    AudioComponentDescription outputcd = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_RemoteIO,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0
    };
    
    // Get the output AudioComponent
    AudioComponent comp = AudioComponentFindNext (NULL, &outputcd);
    if (comp == NULL) {
        printf ("can't get output unit");
        exit (-1);
    }
    
    // Create a new instance of the AudioComponent = the AudioUnit
    status = AudioComponentInstanceNew(comp, &player->outputUnit);
    CheckError (status, "Couldn't open component for outputUnit");
    
    
    // Set the stream format
    status = AudioUnitSetProperty(player->outputUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &CAPAudioDescription,
                                  sizeof(CAPAudioDescription));
    CheckError (status,"kAudioUnitProperty_StreamFormat");

    
    // Set the render callback
    AURenderCallbackStruct input = {
        .inputProc = CAPRenderProc,
        .inputProcRefCon = player
    };
    
    status = AudioUnitSetProperty(player->outputUnit,
                                    kAudioUnitProperty_SetRenderCallback,
                                    kAudioUnitScope_Global,
                                    0,
                                    &input,
                                    sizeof(input));
    CheckError (status, "Could not set render callback");
    
    
    // Set the maximum frames per slice (not necessary)
    UInt32 framesPerSlice = 4096;
    status = AudioUnitSetProperty(player->outputUnit,
                                    kAudioUnitProperty_MaximumFramesPerSlice,
                                    kAudioUnitScope_Global,
                                    0,
                                    &framesPerSlice,
                                  sizeof(framesPerSlice));
    CheckError (status, "AudioUnitSetProperty(kAudioUnitProperty_MaximumFramesPerSlice");
    
    
    // Initialize the Audio Unit
    status = AudioUnitInitialize(player->outputUnit);
    CheckError (status, "Couldn't initialize output unit");
    
    
    // Start the Audio Unit (sound begins)
    status = AudioOutputUnitStart(player->outputUnit);
    CheckError (status, "Couldn't start output unit");
}

void CAPDisposeAudioOutput(CAPAudioOutput *output) {
    AudioOutputUnitStop(output->outputUnit);
    AudioUnitUninitialize(output->outputUnit);
    AudioComponentInstanceDispose(output->outputUnit);
}




