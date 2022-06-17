/*
 * K65 Phenol - CV / GATE Control
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef CV_GATE_CTRL_H
#define CV_GATE_CTRL_H

#define NOTE_LOW 12
#define NOTE_HIGH 115
#define NOTE_STEP_SIZE 31

// init the CV/gate control code
void cv_gate_ctrl_init(void);

// set CV output for note
void cv_gate_ctrl_note(unsigned char chan, unsigned char note, unsigned char on);

// set the CV output bend
void cv_gate_ctrl_bend(unsigned char chan, int bend);

// set CV output for CC / pitch
void cv_gate_ctrl_cv(unsigned char chan, unsigned int val);

// set gate output for CC / pitch
void cv_gate_ctrl_gate(unsigned char chan, unsigned char on);

#endif