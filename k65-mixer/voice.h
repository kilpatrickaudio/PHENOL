/*
 * K65 Phenol - MIDI to CV Voice Control
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 */
// CV modes
#define VOICE_MODE_SINGLE 0
#define VOICE_MODE_SPLIT 1
#define VOICE_MODE_POLY 2
#define VOICE_MODE_ARP 3
#define VOICE_MODE_VELO 4

// init the voice manager
void voice_init(void);

// runs the timer task
void voice_timer_task(void);

// set up the voice mode
void voice_set_mode(unsigned char mode, unsigned char split);

// set up the voice unit
void voice_set_unit(unsigned char unit);

// set the pitch bend range for a voice
void voice_set_pitch_bend_range(unsigned char voice, unsigned char bend);

// set the note offset (transpose) for generating pitches
void voice_set_transpose(unsigned char voice, int transpose);

// note on
void voice_note_on(unsigned char voice, unsigned char note, unsigned char velocity);

// note off
void voice_note_off(unsigned char voice, unsigned char note);

// turn all notes off and reset the prio list
// does note reset the transpose setting
void voice_all_notes_off(unsigned char voice);

// damper pedal
void voice_damper(unsigned char voice, unsigned char state);

// pitch bend
void voice_pitch_bend(unsigned char voice, unsigned int bend);

// reset voice outputs and state
void voice_state_reset(void);
