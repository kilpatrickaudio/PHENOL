/*
 * K65 Phenol - MIDI Handler
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 *
 */
#ifndef PHENOL_MIDI_H
#define PHENOL_MIDI_H

// midi port names
#define MIDI_PORT_DIN 0
#define MIDI_PORT_USB 1

// initialize the MIDI handler
void phenol_midi_init(void);

// reset any modes that were set (for soft power on)
void phenol_midi_reset(void);

// run the timer task to do mode switching
void phenol_midi_timer_task(void);

// get the state of the stop ignore mode
int phenol_midi_get_stop_ignore(void);

// set the state of the stop ignore mode
void phenol_midi_set_stop_ignore(int state);

#endif
