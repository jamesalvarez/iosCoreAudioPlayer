# iosCoreAudioPlayer
A simple iOS Core Audio player to play any file using Audio Unit output and ExtAudioFile

I am learning Core Audio, and I couldn't find an example of:

**a) How to set up the most basic Core Audio output audio stream**

**b) How to load an audio file completely into memory.**

This project basically consists of two C functions, one which loads any audio file ExtAudioFile can read into a struct, and another which plays that data in a loop.  There are no ring buffers or anything more complicated that these tasks.

It is hopefully useful to someone who is just starting out with Core Audio.

I have taken code from TheAmazingAudioEngine, Novocaine and the book Learning Core Audio.