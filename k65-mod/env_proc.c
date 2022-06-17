/*
 * K65 Phenol - Mod Processor Envelope / LFO Routines
 *
 * Copyright 2014: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#include "env_proc.h"
#include "dac.h"
#include "ioctl.h"
#include "switch_filter.h"
#include "lfo_freq.h"
#include "env_freq.h"
#include "clamp.h"
#include "sine.h"
#include "scale.h"
#include "note_to_val.h"
#include "steps.h"
#include "scales.h"
#include <stdlib.h>

// modes
#define ENV_TYPE_AHR 0
#define ENV_TYPE_OSC 1
#define ENV_TYPE_AR 2
#define ENV_MOD_STEPS 0
#define ENV_MOD_DELAY 1
#define ENV_MOD_SCALE 2
#define ENV_OUT_NORMAL 0
#define ENV_OUT_INVERT 1
#define ENV_OUT_ABS 2

// trigger
#define ENV_TRIG_IDLE 0
#define ENV_TRIG_RISING 1
#define ENV_TRIG_HOLDING 2
#define ENV_TRIG_FALLING 3

// state
#define ENV_STATE_IDLE 0
#define ENV_STATE_UP 1
#define ENV_STATE_HOLD 2
#define ENV_STATE_DOWN 3
int env_scan_phase;

// settings and state - mod 1 and 2
#define ENV_NUM_CHANS 2
#define DAC_OFFSET -65
int env12_type[ENV_NUM_CHANS];  // 0-2 = type setting
int env12_mod[ENV_NUM_CHANS];  // 0-2 = mod setting
int env12_out[ENV_NUM_CHANS];  // 0-2 = out setting
int env12_gate[ENV_NUM_CHANS];  // 1 = gate on, 0 = gate off
int env12_trig[ENV_NUM_CHANS];  // 0-3 = trigger state
int env12_trigger_latch[ENV_NUM_CHANS];  // 1 = latched, 0 = normal - used to start osc with no gate applied
// up and down time / speed CV
int env12_up_time_setting[ENV_NUM_CHANS];  // 0-255 = 0-100% time
int env12_down_time_setting[ENV_NUM_CHANS];  // 0-255 = 0-100% time - unused for osc mode
int env12_speed_cv[ENV_NUM_CHANS];  // 0-255 = 0-100% speed from CV input
// steps and delay
int env12_steps_setting[ENV_NUM_CHANS];  // 0-15 = smooth to quant.
int env12_delay_setting[ENV_NUM_CHANS];  // 0-1023 = 0 to max. delay
// level and invert
int env12_level_setting[ENV_NUM_CHANS];  // 0-255 = 0-100% level
int env12_invert[ENV_NUM_CHANS];  // 0 = normal, 1 = invert

// running vars - mod 1 and 2
#define ENV_MAXVAL 1073741824
int env12_delay_count[ENV_NUM_CHANS];   // delay counter
int env12_run_state[ENV_NUM_CHANS];  // running state
int env12_up_freq[ENV_NUM_CHANS];  // freq from speed lookup table
int env12_down_freq[ENV_NUM_CHANS];  // freq from speed lookup table
int env12_acc[ENV_NUM_CHANS];  // current accumulator value

// settings and state - mod 3
#define ENV3_RAND_BREAKPOINT 0xff  // a mask used to set random updates vs. sine
int env3_speed_setting;  // 0-255% = 0-100% speed
int env3_freq;  // freq from speed lookup table

// running vars - mod 3
int env3_acc;  // current accumulator value
int env3_rand;  // keeps track of random output state

// local functions
void env_proc_set_type(int chan, int type);
void env_proc_set_mod(int chan, int mod);
void env_proc_set_out(int chan, int out);
void env_proc_update_leds(int chan);
void env_proc_run12(int chan);
void env_proc_run3(void);
void env_proc_reset(int chan);

// init the env proc
void env_proc_init(void) {
	int i;

	// init the scale processor
	scale_init();

	// reset outputs
	dac_write_dac(DAC_MOD1, 0x7ff);
	dac_write_dac(DAC_MOD2, 0x7ff);
	dac_write_dac(DAC_MOD3_RAND, 0x7ff);
	dac_write_dac(DAC_MOD3_SINE, 0x7ff);

	// reset stuff
	for(i = 0; i < ENV_NUM_CHANS; i ++) {
		env12_delay_count[i] = 0;
		env12_run_state[i] = ENV_STATE_IDLE;
		env12_trig[i] = ENV_TRIG_IDLE;
		env12_trigger_latch[i] = 0;  // not latched
		env12_gate[i] = 0;
		env12_up_freq[i] = lfo_freq_table[0];
		env12_down_freq[i] = lfo_freq_table[0];
		env12_acc[i] = 0;
		env12_type[i] = ENV_TYPE_AHR;
		env12_mod[i] = ENV_MOD_STEPS;
		env12_out[i] = ENV_OUT_NORMAL;
		env_proc_reset(i);
		env_proc_update_leds(i);
	}
	env3_freq = lfo_freq_table[0];
	env3_acc = 0;

	srand(0xbababa);
}

// run the timer task
void env_proc_timer_task(void) {
	int sw;
	int val;
	int temp;
	int i;

	// check gate inputs
	for(i = 0; i < ENV_NUM_CHANS; i ++) {
		// gate triggered
		if(ioctl_get_gate_in(i) || ioctl_get_gate_sw(i)) {
			env12_trigger_latch[i] = 0;  // force reset
			// timeout delay
			if(env12_mod[i] == ENV_MOD_DELAY && env12_delay_count[i] > 0) {
				env12_delay_count[i] --;
				continue;
			}
			env12_gate[i] = 1;
			ioctl_set_gate_led(i, 1);
		}
		// gate latched
		else if(env12_trigger_latch[i]) {
			env12_gate[i] = 1;
			ioctl_set_gate_led(i, 1);
		}
		// gate released
		else {
			env12_gate[i] = 0;
			ioctl_set_gate_led(i, 0);
			// delay mode - get the delay setting
			if(env12_mod[i] == ENV_MOD_DELAY) {
				env12_delay_count[i] = env12_delay_setting[i];
			}
		}
	}

	// scan various input parameters
	switch(env_scan_phase & 0x03) {
		case 0:
			// pot 1
			for(i = 0; i < ENV_NUM_CHANS; i ++) {
				// up time (AR and AHR modes, speed (OSC mode)
				env12_up_time_setting[i] = ioctl_get_pot(POT_MOD1_POT1 + (i * POTS_PER_BANK));
			}
			break;
		case 1:
			// pot 2
			for(i = 0; i < ENV_NUM_CHANS; i ++) {
				switch(env12_type[i]) {
					case ENV_TYPE_AR:
					case ENV_TYPE_AHR:
						env12_down_time_setting[i] = ioctl_get_pot(POT_MOD1_POT2 + (i * POTS_PER_BANK));
						break;
					case ENV_TYPE_OSC:
						env12_level_setting[i] = ioctl_get_pot(POT_MOD1_POT2 + (i * POTS_PER_BANK));
						break;
				}
			}
			break;
		case 2:
			// pot 3
			for(i = 0; i < ENV_NUM_CHANS; i ++) {
				// steps - steps amount
				switch(env12_mod[i]) {
					case ENV_MOD_STEPS:
					case ENV_MOD_SCALE:
						env12_steps_setting[i] = (ioctl_get_pot(POT_MOD1_POT3 + (i * POTS_PER_BANK))) >> 4;
						break;
					case ENV_MOD_DELAY:
						env12_delay_setting[i] = ioctl_get_pot(POT_MOD1_POT3 + (i * POTS_PER_BANK)) << 4;
						break;
				}
			}			
			break;
		case 3:
			// speed CV in
			for(i = 0; i < ENV_NUM_CHANS; i ++) {
				env12_speed_cv[i] = ioctl_get_cv(i) >> 4;
			}
			// mod 3 speed pot
			env3_speed_setting = ioctl_get_pot(POT_MOD3_SPEED);
			break;
	}
	env_scan_phase ++;

	// update frequencies
	for(i = 0; i < ENV_NUM_CHANS; i ++) {
		switch(env12_type[i]) {
			case ENV_TYPE_AR:
			case ENV_TYPE_AHR:
				temp = env12_speed_cv[i] - 128;  // -128 to +127
				temp += env12_up_time_setting[i];
				env12_up_freq[i] = env_freq_table[clamp(temp, 0, 255)];
				temp = env12_speed_cv[i] - 128;  // -128 to +127
				temp += env12_down_time_setting[i];
				env12_down_freq[i] = env_freq_table[clamp(temp, 0, 255)];
				break;
			case ENV_TYPE_OSC:
				temp = env12_speed_cv[i] - 128;  // -128 to +127
				temp += env12_up_time_setting[i];
				env12_up_freq[i] = lfo_freq_table[clamp(temp, 0, 255)];
				env12_down_freq[i] = env12_up_freq[i];
				break;
		}
	}
	env3_freq = lfo_freq_table[env3_speed_setting];

	// handle switches
 	sw = switch_filter_get_event();
	if(sw) {
		val = sw & 0xf000;
		sw = (sw & 0x0fff);
		// key down events
		if(val == SW_CHANGE_PRESSED) {
			switch(sw) {
				case SW_MOD1_SW1:  // type
					env12_type[0] = (env12_type[0] + 1) % 3;
					env_proc_reset(0);
					env_proc_update_leds(0);
					break;
				case SW_MOD1_SW2:  // mod
					env12_mod[0] = (env12_mod[0] + 1) % 3;
					env_proc_update_leds(0);
					break;
				case SW_MOD1_SW3:  // out
					env12_out[0] = (env12_out[0] + 1) % 3;
					env_proc_update_leds(0);
					break;
				case SW_MOD2_SW1:  // type
					env12_type[1] = (env12_type[1] + 1) % 3;
					env_proc_reset(1);
					env_proc_update_leds(1);
					break;
				case SW_MOD2_SW2:  // mod
					env12_mod[1] = (env12_mod[1] + 1) % 3;
					env_proc_update_leds(1);
					break;
				case SW_MOD2_SW3:  // out
					env12_out[1] = (env12_out[1] + 1) % 3;
					env_proc_update_leds(1);
					break;
			}
		}
	}
}

// run the sample task
void env_proc_sample_task(void) {
	int i;
	// run each channel
	for(i = 0; i < ENV_NUM_CHANS; i ++) {
		env_proc_run12(i);
	}
	env_proc_run3();
}

// update the mode LEDs
void env_proc_update_leds(int chan) {
	if(chan < 0 || chan > 1) {
		return;
	}
	ioctl_set_mode_led(chan, 0, env12_type[chan]);
	ioctl_set_mode_led(chan, 1, env12_mod[chan]);
	ioctl_set_mode_led(chan, 2, env12_out[chan]);
}

// run envelope processors for mod 1 and 2
void env_proc_run12(int chan) {
	int temp, temp2;
	int oct, semi;
	if(chan < 0 || chan > 1) {
		return;
	}

	// compute the trigger state
	if(env12_trig[chan] == ENV_TRIG_IDLE) {
		if(env12_gate[chan]) {
			env12_trig[chan] = ENV_TRIG_RISING;
		}
	}
	else if(env12_trig[chan] == ENV_TRIG_RISING) {
		env12_trig[chan] = ENV_TRIG_HOLDING;
	}
	else if(env12_trig[chan] == ENV_TRIG_HOLDING) {
		if(!env12_gate[chan]) {
			env12_trig[chan] = ENV_TRIG_FALLING;
		}
	}
	else if(env12_trig[chan] == ENV_TRIG_FALLING) {
		env12_trig[chan] = ENV_TRIG_IDLE;
	}

	// do ramp calculations
	switch(env12_run_state[chan]) {
		case ENV_STATE_UP:
			env12_acc[chan] += env12_up_freq[chan];
			// reached top
			if(env12_acc[chan] > ENV_MAXVAL) {
				switch(env12_type[chan]) {
					case ENV_TYPE_AR:
						env12_run_state[chan] = ENV_STATE_DOWN;
						break;
					case ENV_TYPE_AHR:
						env12_run_state[chan] = ENV_STATE_HOLD;
						break;
					case ENV_TYPE_OSC:
						env12_run_state[chan] = ENV_STATE_DOWN;
						break;
				}
			}
			// trigger was released in AHR mode
			else if(env12_type[chan] == ENV_TYPE_AHR && env12_trig[chan] == ENV_TRIG_FALLING) {
				env12_run_state[chan] = ENV_STATE_DOWN;
			}
			break;
		case ENV_STATE_HOLD:
			env12_acc[chan] = ENV_MAXVAL;
//			if(env12_trig[chan] == ENV_TRIG_FALLING) {
            // 1.23 fix - make sure we don't get stuck at the top when changing modes while running
			if(env12_trig[chan] == ENV_TRIG_FALLING ||
                    env12_trig[chan] == ENV_TRIG_IDLE) {
				env12_run_state[chan] = ENV_STATE_DOWN;
			}
			break;
		case ENV_STATE_DOWN:
			env12_acc[chan] -= env12_down_freq[chan];
			// reached bottom
			if(env12_acc[chan] < 0) {
				switch(env12_type[chan]) {
					case ENV_TYPE_AR:
					case ENV_TYPE_AHR:
						env12_run_state[chan] = ENV_STATE_IDLE;
						break;
					case ENV_TYPE_OSC:
						if(env12_trig[chan] == ENV_TRIG_HOLDING) {
							env12_run_state[chan] = ENV_STATE_UP;
						}
						else {
							env12_run_state[chan] = ENV_STATE_IDLE;
						}
						break;
				}
			}
			// trigger was applied in env mode
			else if((env12_type[chan] == ENV_TYPE_AR ||
						env12_type[chan] == ENV_TYPE_AHR) && env12_trig[chan] == ENV_TRIG_RISING) {
				env12_run_state[chan] = ENV_STATE_UP;
			}
			break;
		case ENV_STATE_IDLE:
		default:
			if(env12_trig[chan] == ENV_TRIG_RISING) {
				env12_run_state[chan] = ENV_STATE_UP;
			}
			break;
	}
	temp = clamp(env12_acc[chan] >> 18, 0x000, 0xfff);

	// mod / level mode
	switch(env12_mod[chan]) {
		case ENV_MOD_STEPS:
			temp2 = env12_steps_setting[chan];  // step pot (0-15)
			// use steps table
			if(temp2 < 14) {
				temp = scale_find_note(temp);  // convert value into note (0-120)
				temp = steps[temp2][temp];  // look up the quantization
				temp = note_to_val[temp];  // convert note back to value (0-4095)
			}
			// otherwise just pass through the raw value
			break;
		case ENV_MOD_SCALE:
			// do scaling of input for osc mode
			if(env12_type[chan] == ENV_TYPE_OSC) {
				temp = (temp * env12_level_setting[chan]) >> 8;  // scale
				if(env12_out[chan] != ENV_OUT_ABS) {
					temp += 0x7ff - (env12_level_setting[chan] << 3);  // offset
				}
			}
			// abs output precalc
			if(env12_out[chan] == ENV_OUT_ABS) {
				temp = (temp >> 1) + 0x7ff;
			}
			temp2 = env12_steps_setting[chan];  // step pot (0-15)
			oct = scale_find_octave(temp);  // find octave
			semi = scale_find_semitone(oct, temp);  // find semitone
			semi = scales[temp2][semi];  // look up the quantization
			temp = note_to_val[(oct * 12) + semi];  // convert note back to value (0-4095)
			// abs output postcalc
			if(env12_out[chan] == ENV_OUT_ABS) {
				temp += DAC_OFFSET;
			}
			// invert output postcalc
			else if(env12_out[chan] == ENV_OUT_INVERT) {
				temp = 0xfff - temp;
			}
			break;
	}

	// output processing for non-scale modes
	if(env12_mod[chan] != ENV_MOD_SCALE) {
		// osc mode level - not for scale mode
		if(env12_type[chan] == ENV_TYPE_OSC) {
			temp = (temp * env12_level_setting[chan]) >> 8;  // scale
			if(env12_out[chan] != ENV_OUT_ABS) {
				temp += 0x7ff - (env12_level_setting[chan] << 3);  // offset
			}
		}
		// output processing
		switch(env12_out[chan]) {
			case ENV_OUT_INVERT:
				temp = 0xfff - temp;
				break;
			case ENV_OUT_ABS:
				temp = (temp >> 1) + 0x7ff + DAC_OFFSET;  // zero it on the centre line
				break;
		}
		
	}

	// output DAC
	dac_write_dac(DAC_MOD1 + chan, clamp(temp, 0x000, 0xfff));
}

// run the process for mod 3
void env_proc_run3(void) {
	int temp;	
	env3_acc += env3_freq;
	temp = clamp((env3_acc >> 19) & 0xfff, 0, 0xfff);

	// sine
	dac_write_dac(DAC_MOD3_SINE, sine1024[temp >> 2]);

	// noise output
	if(env3_speed_setting > 253) {
		dac_write_dac(DAC_MOD3_RAND, rand() & 0xfff);
	}
	// random output
	else {
		temp = temp & (ENV3_RAND_BREAKPOINT << 1);
		if(env3_rand && (temp > ENV3_RAND_BREAKPOINT)) {
			dac_write_dac(DAC_MOD3_RAND, rand() & 0xfff);
			env3_rand = 0;
		}
		else if(temp < ENV3_RAND_BREAKPOINT) {
			env3_rand = 1;
		}
	}
}

// reset stuff based on the type selected
void env_proc_reset(int chan) {
	if(chan < 0 || chan > 1) {
		return;
	}
	// reset the type
	switch(env12_type[chan]) {
		case ENV_TYPE_AR:
		case ENV_TYPE_AHR:
			env12_trigger_latch[chan] = 0;  // not latched
			env12_level_setting[chan] = 255;  // 100% level
			break;
		case ENV_TYPE_OSC:
			env12_trigger_latch[chan] = 1;  // latched to start
			env12_level_setting[chan] = 0;  // muted until we poll again
			break;
	}

	// reset mod
	env12_steps_setting[chan] = 0;  // wait for poll
	env12_delay_setting[chan] = 0;  // wait for poll

	// set the output
	switch(env12_out[chan]) {
		case ENV_OUT_NORMAL:
			env12_invert[chan] = 0;  // normal
			break;
		case ENV_OUT_INVERT:
			env12_invert[chan] = 1;  // inverted
			break;
		case ENV_OUT_ABS:
			break;
	}
}
