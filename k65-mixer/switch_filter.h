/*
 * Switch Filter
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 *
 */
#ifndef SWITCH_FILTER_H
#define SWITCH_FILTER_H

#include <inttypes.h>

#define SW_NUM_INPUTS 2

// switch change flags
#define SW_CHANGE_UNPRESSED 0x1000
#define SW_CHANGE_PRESSED 0x2000
#define SW_CHANGE_ENC_MOVE_CW 0x3000
#define SW_CHANGE_ENC_MOVE_CCW 0x4000

// initialize the filter code
void switch_filter_init(uint16_t sw_timeout, 
		uint16_t sw_debounce, uint16_t enc_timeout);

// set a pair of channels an an encoder - must be sequential 
void switch_filter_set_encoder(uint16_t start_chan);

// record a new sample value
void switch_filter_set_val(uint16_t start_chan, 
		uint16_t num_chans, int states);

// clear all state
void switch_filter_clear_state(void);

// get the next event in the queue
int switch_filter_get_event(void);

#endif
