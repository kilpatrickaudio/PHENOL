/*
 * K65 Phenol Mixer - Audio Processing
 *
 * Copyright 2014: Kilpatrick 
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef AUDIO_PROC_H
#define AUDIO_PROC_H

// init the audio processor
void audio_proc_init(void);

// process a buffer of samples
void audio_proc_process(void);

// generate silent output
void audio_proc_silence(void);

#endif
