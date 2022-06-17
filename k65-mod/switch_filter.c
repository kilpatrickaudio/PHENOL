/*
 * Switch Filter
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include "switch_filter.h"
#include "HardwareProfile.h"
#include <stdlib.h>

// settings
int switch_sw_timeout;
int switch_sw_debounce;
int switch_enc_timeout;
struct switch_state {
	uint16_t state_count;
	uint16_t timeout;
	uint16_t change_f;
	uint8_t mode;
};
struct switch_state sw_state[SW_NUM_INPUTS];
// switch modes
#define SW_MODE_BUTTON 1
#define SW_MODE_ENC_A 2
#define SW_MODE_ENC_B 3
// event queue
#define SW_EVENT_QUEUE_LEN 64
#define SW_EVENT_QUEUE_MASK (SW_EVENT_QUEUE_LEN - 1)
uint16_t switch_event_queue[SW_EVENT_QUEUE_LEN];
int switch_event_inp;
int switch_event_outp;

// initialize the debounce code
void switch_filter_init(uint16_t sw_timeout, uint16_t sw_debounce, 
		uint16_t enc_timeout) {
	// settings
	switch_sw_timeout = sw_timeout;
	switch_sw_debounce = sw_debounce;
	switch_enc_timeout = enc_timeout;
	// reset stuff
	switch_filter_clear_state();
}

// set a pair of channels an an encoder - must be sequential 
void switch_filter_set_encoder(uint16_t start_chan) {
	if(start_chan >= (SW_NUM_INPUTS - 1)) {
		return;
	}
	sw_state[start_chan].mode = SW_MODE_ENC_A;
	sw_state[start_chan].change_f = 0;
	sw_state[start_chan + 1].mode = SW_MODE_ENC_B;
	sw_state[start_chan + 1].change_f = 0;
}

// record a new sample value
void switch_filter_set_val(uint16_t start_chan, uint16_t num_chans, int states) {
	int i, temp, old_val;
	if((start_chan + num_chans) > SW_NUM_INPUTS) {
		return;
	}

	temp = states;

	// process each switch in the input
	for(i = start_chan; i < (start_chan + num_chans); i ++) {
		// we're in a timeout phase to limit switch speed
		if(sw_state[i].timeout) {
			sw_state[i].timeout --;
			temp = (temp >> 1);
			continue;
		}

		// buttons
		if(sw_state[i].mode == SW_MODE_BUTTON) {
			// switch is pressed
			if(temp & 0x01) {
				// register press
				if(sw_state[i].change_f == SW_CHANGE_UNPRESSED && sw_state[i].state_count < switch_sw_debounce) {
					sw_state[i].state_count ++;
					if(sw_state[i].state_count == switch_sw_debounce) {
						sw_state[i].change_f = SW_CHANGE_PRESSED;
						switch_event_queue[switch_event_inp] = SW_CHANGE_PRESSED | (i & 0xfff);
						switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
						sw_state[i].timeout = switch_sw_timeout;
					}
				}
			}
			// switch is unpressed
			else {
				// register release
				if(sw_state[i].change_f == SW_CHANGE_PRESSED && sw_state[i].state_count) {
					sw_state[i].state_count --;
					if(sw_state[i].state_count == 0) {
						sw_state[i].change_f = SW_CHANGE_UNPRESSED;
						switch_event_queue[switch_event_inp] = SW_CHANGE_UNPRESSED | (i & 0xfff);
						switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
						sw_state[i].timeout = switch_sw_timeout;
					}
				}
			}
		}
		// encoders
		else if(sw_state[i].mode == SW_MODE_ENC_A) {
			old_val = sw_state[i].change_f;
			// switch closed
			if(temp & 0x01) {
				sw_state[i].change_f |= 0x01;
			}
			// switch opened
			else {
				sw_state[i].change_f &= ~0x01;
			}
			// did switch move?
			if(old_val != sw_state[i].change_f) {
				if(sw_state[i].change_f == 0 && old_val == 1) {
					// generate event
					switch_event_queue[switch_event_inp] = SW_CHANGE_ENC_MOVE_CW | (i & 0xfff);
					switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
				}
			}
			sw_state[i].timeout = switch_enc_timeout;
		}
		else if(sw_state[i].mode == SW_MODE_ENC_B) {
			old_val = sw_state[i-1].change_f;
			// switch closed
			if(temp & 0x01) {
				sw_state[i-1].change_f |= 0x02;
			}
			// switch opened
			else {
				sw_state[i-1].change_f &= ~0x02;
			}
			// did switch move?
			if(old_val != sw_state[i-1].change_f) {
				if(sw_state[i-1].change_f == 0 && old_val == 2) {
					// generate event
					switch_event_queue[switch_event_inp] = SW_CHANGE_ENC_MOVE_CCW | ((i - 1) & 0xfff);
					switch_event_inp = (switch_event_inp + 1) & SW_EVENT_QUEUE_MASK;
				}
			}
			sw_state[i].timeout = switch_enc_timeout;
		}
		temp = (temp >> 1);
	}
}

// clear all state
void switch_filter_clear_state(void) {
	int i;
	for(i = 0; i < SW_NUM_INPUTS; i ++) {
		sw_state[i].state_count = 0;
		sw_state[i].timeout = 0;
		sw_state[i].change_f = SW_CHANGE_UNPRESSED;
		sw_state[i].mode = SW_MODE_BUTTON;  // default
	}
	for(i = 0; i < SW_EVENT_QUEUE_LEN; i ++) {
		switch_event_queue[i] = 0;
	}
	switch_event_inp = 0;
	switch_event_outp = 0;
}

// get the next event in the queue
int switch_filter_get_event(void) {
	int temp;
	if(switch_event_inp == switch_event_outp) {
		return 0;
	}
	temp = switch_event_queue[switch_event_outp];
	switch_event_outp = (switch_event_outp + 1) & SW_EVENT_QUEUE_MASK;
	return temp;
}