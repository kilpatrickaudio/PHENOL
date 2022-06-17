/*
 * PHENOL Mini Sequencer
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef SEQ_H
#define SEQ_H

// init the sequencer
void seq_init(void);

// run the timer task - 1000us
void seq_timer_task(void);

// handle the record button
void seq_rec_button(int pressed);

// handle the play button
void seq_play_button(int pressed);

// handle a MIDI start event
void seq_clock_handle_midi_start(void);

// handle a MIDI stop event
void seq_clock_handle_midi_stop(void);

// handle a MIDI continue event
void seq_clock_handle_midi_continue(void);

// handle a MIDI clock fail event
void seq_clock_handle_clock_fail(void);

// handle a clock tick
void seq_clock_tick(void);

//
// MIDI handlers - sequencer-specific messages
// pass through this module always
//
// note off
void seq_midi_note_off(int note);

// note on
void seq_midi_note_on(int note);

// pitch bend - bend = 14 bit signed value
void seq_midi_pitch_bend(int bend);

// sustain pedal
void seq_midi_sustain_pedal(int value);

#endif

