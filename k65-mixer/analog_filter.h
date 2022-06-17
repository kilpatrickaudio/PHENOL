/*
 * Analog Filtering
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef ANALOG_FILTER_H
#define ANALOG_FILTER_H

#define ANALOG_CHANS 8
#define ANALOG_MAXVAL 0xfff
#define ANALOG_MINVAL 0x000
#define ANALOG_MAXTH 0xfd0
#define ANALOG_MINTH 0x020

// defaults
#define ANALOG_WINDOW_THRESH 32
#define ANALOG_SMOOTHING 0  // no smoothing

// initialize the analog filter code
void analog_filter_init(void);

// set the analog window setting for a channel
void analog_filter_set_window(unsigned char chan, int val);

// set the smoothing amount for a channel - 0-255 = 0-100%
void analog_filter_set_smoothing(unsigned char chan, int val);

// record a new sample value
void analog_filter_set_val(unsigned char chan, int newval);

// get a value
unsigned int analog_filter_get_val(unsigned char chan);

#endif
