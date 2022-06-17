/*
 * PHENOL MIDI Clock Module
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef MIDI_CLOCK_H
#define MIDI_CLOCK_H

// init the MIDI clock
void midi_clock_init(void);

// run the MIDI clock timer task
void midi_clock_timer_task(void);

// check if the clock is running on internal - 1 = internal, 0 = external
int midi_clock_get_internal(void);

// pulse the clock output
void midi_clock_pulse_clock_out(void);

// set the internal clock tempo in BPM
void midi_clock_set_tempo(float tempo);

// set the external clock divider - 0-3 = 1/1, 1/2, 1/3, 1/4
void midi_clock_set_clock_div(int div);

// ungate the external clock because play was pressed
void midi_clock_ungate(void);

//
// MIDI handlers
//
// handle MIDI song position
void midi_clock_rx_song_position(unsigned int pos);

// handle MIDI timing tick
void midi_clock_rx_timing_tick(void);

// handle MIDI start song
void midi_clock_rx_start_song(void);

// handle MIDI stop song
void midi_clock_rx_stop_song(void);

// handle MIDI continue song
void midi_clock_rx_continue_song(void);

#endif
