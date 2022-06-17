/*
 * K65 Phenol Mixer - Pulse Divider
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include "pulse_div.h"
#include "ioctl.h"

int pulse_count;
int pulse_in_old;

// local functions
void pulse_div_output(void);

// init the pulse divider
void pulse_div_init(void) {
	pulse_count = 0;
	pulse_in_old = 0;
}

// run the pulse divider timer task
void pulse_div_timer_task(void) {
	int temp;
	temp = ioctl_get_divider_in();
	// no change
	if(temp == pulse_in_old) {
		return;
	}
	// rising edge
	if(temp) {
		pulse_count ++;
		pulse_div_output();
	}
	pulse_in_old = temp;
}

// reset the pulse divider outputs and count
void pulse_div_reset(void) {
	pulse_count = 0;
	pulse_div_output();
}

//
// local functions
//
void pulse_div_output(void) {
	int a, b, c, d;
	a = 0;
	b = 0;
	c = 0;
	d = 0;
	if(pulse_count & 0x01) {
		a = 1;
	}
	if(pulse_count % 3) {
		b = 1;
	}
	if(pulse_count & 0x02) {
		c = 1;
	}
	if((pulse_count % 6) > 2) {
		d = 1;
	}
	ioctl_set_divider_outputs(a, b, c, d);	
}
