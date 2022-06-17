/*
 * PHENOL MIDI Clock Module
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#include "midi_clock.h"
#include "midi.h"
#include "phenol_midi.h"
#include "seq.h"
#include "ioctl.h"
#include "pulse_div.h"
#include <inttypes.h>

//#define MIDI_CLOCK_MIDI_DEBUG
#define MIDI_CLOCK_TIMEOUT_TIME 250  // timeout period in ms where we deem MIDI clock failed
#define MIDI_CLOCK_PULSE_TIME 20
int midi_clock_tick_count;  // a count of clock ticks
int midi_clock_run;  // clock gate based on MIDI start, continue, stop - 1 = run, 0 = stop
int midi_clock_timeout;  // timeout for MIDI clock fail detect
int midi_clock_internal;  // 1 = internal, 0 = external
int midi_clock_div_setting;  // external clock divider - 0-3 = 1/1, 1/2, 1/3, 1/4
int midi_clock_div_count;  // external clock divider counter
// internal clock stuff
#define MIDI_CLOCK_TASK_INTERVAL_US 1000
int midi_clock_time_count;  // a count of real time
int midi_clock_next_tick_time;  // time for the next tick
int midi_clock_us_per_tick;  // the number of us per tick

// init the MIDI clock
void midi_clock_init(void) {
	midi_clock_tick_count = 0;
	midi_clock_run = 1;  // default running
    midi_clock_timeout = 0;  // failed
    midi_clock_internal = 1;  // internal
    midi_clock_time_count = 0;
    midi_clock_next_tick_time = 0;
    midi_clock_set_tempo(96.0);
    midi_clock_set_clock_div(0);  // div = 1/1
    midi_clock_div_count = 0;
}

// run the MIDI clock timer task - 1000us
void midi_clock_timer_task(void) {
    // handle clock switching - external MIDI vs. internal clock gen
    if(midi_clock_timeout) {
        midi_clock_internal = 0;  // internal clock disabled
        midi_clock_timeout --;

        // external MIDI clock failed
        if(midi_clock_timeout == 0) {
#ifdef MIDI_CLOCK_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "MIDI external clock failed");
#endif
            // stop sequencer playback
            seq_clock_handle_clock_fail();
            midi_clock_internal = 1;  // internal clock enabled
        }
    }

    // run internal clock
    if(midi_clock_internal) {
        midi_clock_time_count += MIDI_CLOCK_TASK_INTERVAL_US;
        // decide if we should issue a clock
        if(midi_clock_time_count > midi_clock_next_tick_time) {
            seq_clock_tick();  // sequencer clock
            midi_clock_tick_count ++;
            midi_clock_next_tick_time += midi_clock_us_per_tick;
        }
    }
}

// check if the clock is running on internal - 1 = internal, 0 = external
int midi_clock_get_internal(void) {
    return midi_clock_internal;
}

// pulse the clock output - used by sequencer for internal clock
void midi_clock_pulse_clock_out(void) {
    ioctl_set_midi_clock_out(MIDI_CLOCK_PULSE_TIME);
}

// set the internal clock tempo in BPM
void midi_clock_set_tempo(float tempo) {
    midi_clock_us_per_tick = (uint64_t)(60000000.0 / (tempo * 24.0));
}

// set the external clock divider - 0-3 = 1/0.5, 1/1, 1/2, 1/3
void midi_clock_set_clock_div(int div) {
    midi_clock_div_setting = div & 0x03;
}

// ungate the external clock because play was pressed
void midi_clock_ungate(void) {
    if(!midi_clock_internal) {
        midi_clock_rx_continue_song();  // fake a continue message
    }
}

//
// MIDI handlers
//
// handle MIDI song position
void midi_clock_rx_song_position(unsigned int pos) {
	midi_clock_tick_count = (pos * 6) % 24;
    midi_clock_div_count = 0;
}

// handle MIDI timing tick
void midi_clock_rx_timing_tick(void) {
    midi_clock_timeout = MIDI_CLOCK_TIMEOUT_TIME;  // reset clock timeout

	if(!midi_clock_run) {
		return;
	}
    if(midi_clock_div_count == 0) {
	    if((midi_clock_tick_count % 6) == 0) {
		    midi_clock_pulse_clock_out();
	    }
        seq_clock_tick();  // sequencer clock
        midi_clock_div_count = 0;
    	midi_clock_tick_count ++;
    	if(midi_clock_tick_count == 24) {
	    	midi_clock_tick_count = 0;
	    }
    }
    midi_clock_div_count ++;
    if(midi_clock_div_count > midi_clock_div_setting) {
        midi_clock_div_count = 0;
    }
}

// handle MIDI start song
void midi_clock_rx_start_song(void) {
    midi_clock_timeout = MIDI_CLOCK_TIMEOUT_TIME;  // reset clock timeout
	midi_clock_tick_count = 0;
    midi_clock_div_count = 0;
	midi_clock_run = 1;
	// reset the clock divider
	pulse_div_reset();
    seq_clock_handle_midi_start();  // cause sequencer to start
}

// handle MIDI stop song
void midi_clock_rx_stop_song(void) {
    midi_clock_timeout = MIDI_CLOCK_TIMEOUT_TIME;  // reset clock timeout
	midi_clock_run = 0;
    seq_clock_handle_midi_stop();
}

// handle MIDI continue song
void midi_clock_rx_continue_song(void) {
    midi_clock_timeout = MIDI_CLOCK_TIMEOUT_TIME;  // reset clock timeout
	midi_clock_run = 1;
    seq_clock_handle_midi_continue();
}
