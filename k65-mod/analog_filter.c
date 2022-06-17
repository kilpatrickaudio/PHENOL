/*
 * Analog Filtering
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include "analog_filter.h"

int analog_vals[ANALOG_CHANS];
int analog_outs[ANALOG_CHANS];
int analog_window[ANALOG_CHANS];
int analog_smoothing[ANALOG_CHANS];

// initialize the analog hysteris code
void analog_filter_init(void) {
	int i;
	for(i = 0; i < ANALOG_CHANS; i ++) {
		analog_vals[i] = 0;
		analog_outs[i] = 0;
		analog_window[i] = ANALOG_WINDOW_THRESH;
		analog_smoothing[i] = ANALOG_SMOOTHING;
	}
}

// set the analog window setting for a channel
void analog_filter_set_window(unsigned char chan, int val) {
	if(chan >= ANALOG_CHANS) {
		return;
	}
	analog_window[chan] = val;
}

// set the smoothing amount for a channel
void analog_filter_set_smoothing(unsigned char chan, int val) {
	if(chan >= ANALOG_CHANS) {
		return;
	}
	analog_smoothing[chan] = val;
}

// record a new sample value
void analog_filter_set_val(unsigned char chan, int newval) {
	int temp, smooth;
	if(chan >= ANALOG_CHANS) {
		return;
	}
	// smoothing
	smooth = analog_smoothing[chan];
	temp = ((analog_vals[chan] * smooth) + (newval * (255 - smooth))) >> 8;
	analog_vals[chan] = temp;

	// windowing
	if(temp <= ANALOG_MINVAL) {
		analog_outs[chan] = ANALOG_MINVAL;
	}
	else if(temp >= ANALOG_MAXVAL) {
		analog_outs[chan] = ANALOG_MAXVAL;
	}
	else if(((temp - analog_outs[chan]) & 0xffff) > analog_window[chan]) {
		analog_outs[chan] = temp;
	}
}

// get a value
unsigned int analog_filter_get_val(unsigned char chan) {
	if(chan >= ANALOG_CHANS) {
		return 0;
	}
	return analog_outs[chan];
}

