/*
 * K65 Phenol - Scale Processing
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef SCALE_H
#define SCALE_H

// init the scale
void scale_init(void);

// find the scale octave of the current value
int scale_find_octave(int val);

// find the scale semitone in the current octave
int scale_find_semitone(int oct, int val);

// find the note (0-120) of the value (0-4095)
int scale_find_note(int val);

#endif
