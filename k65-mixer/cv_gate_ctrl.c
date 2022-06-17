/*
 * K1 Mixer Interface - CV / GATE Control
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 */
#include "cv_gate_ctrl.h"
#include "ioctl.h"
#include "utils.h"

#define NOTE_TABLE_LEN 128
#define DAC_OFFSET 79
#define CV_DAC_HI 3800
unsigned int note_lookup[NOTE_TABLE_LEN];
int bend1;
int bend2;
int dac1_cur;  // the current DAC value not including the bend
int dac2_cur;  // the current DAC value not including the bend

// init the CV/gate control code
void cv_gate_ctrl_init(void) {
	int i;
	int step_pos;
	// make the note lookup table
	step_pos = DAC_OFFSET;
	for(i = 0; i < NOTE_TABLE_LEN; i ++) {
		note_lookup[i] = step_pos;
		step_pos += NOTE_STEP_SIZE;
	}

	bend1 = 0;
	bend2 = 0;
	cv_gate_ctrl_note(0, 60, 0);
	cv_gate_ctrl_note(1, 60, 0);
}

// set CV output
void cv_gate_ctrl_note(unsigned char chan, unsigned char note, unsigned char on) {
	if(chan > 1) return;
	if(note < NOTE_LOW || note > NOTE_HIGH) return;

	// note on
	if(on) {
		// CV2 / GATE2
		if(chan) {
//			dac2_cur = note_lookup[note];
//			ioctl_set_midi_cv_out(1, clamp(dac2_cur + bend2, 0, 4095));
//			ioctl_set_midi_clock_out(1);  // clock used as gate 2
		}
		// CV1 / GATE1
		else {
			dac1_cur = note_lookup[note];
			ioctl_set_midi_cv_out(0, clamp(dac1_cur + bend1, 0, 4095));
			ioctl_set_midi_gate_out(1);
		}
	}
	// note off
	else {
		// CV2 / GATE2
		if(chan) {
//			ioctl_set_midi_clock_out(0);  // clock used as gate 2
		}
		// CV1 / GATE1
		else {
			ioctl_set_midi_gate_out(0);
		}
	}
}

// set the CV output bend
void cv_gate_ctrl_bend(unsigned char chan, int bend) {
	int temp;
	if(chan) {
		bend2 = bend;
		temp = dac2_cur + bend2;
		ioctl_set_midi_cv_out(1, clamp(dac2_cur + bend2, 0, 4095));
	}
	else {
		bend1 = bend;
		ioctl_set_midi_cv_out(0, clamp(dac1_cur + bend1, 0, 4095));
	}
}

// set CV output for CC direct / pitch bend - output is clamped to -5V to +5V range
void cv_gate_ctrl_cv(unsigned char chan, unsigned int val) {
	if(chan) {
		dac2_cur = (val & 0xfff) + DAC_OFFSET;
		if(dac2_cur > CV_DAC_HI) dac2_cur = CV_DAC_HI;		
		ioctl_set_midi_cv_out(1, dac2_cur);
	}
	else {
		dac1_cur = (val & 0xfff) + DAC_OFFSET;
		if(dac1_cur > CV_DAC_HI) dac1_cur = CV_DAC_HI;
		ioctl_set_midi_cv_out(0, dac1_cur);
	}
}

//set the gate output for CC / pitch bend
void cv_gate_ctrl_gate(unsigned char chan, unsigned char on) {
	unsigned char val = 0;
	if(on) val = 255;
	if(chan) {
//		ioctl_set_midi_clock_out(on);  // clock used as gate 2
	}
	else {
		ioctl_set_midi_gate_out(on);
	}
}
