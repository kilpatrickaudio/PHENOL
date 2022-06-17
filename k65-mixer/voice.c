/*
 * K1 Mixer Interface - Voice Manager
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.2
 * 
 */
#include "voice.h"
//#include "config_store.h"
#include "cv_gate_ctrl.h"

// settings
unsigned char voice_mode;  // the voice mode
unsigned char voice_split;  // split point
unsigned char voice_unit;  // voice unit number
int voice_bend1_interval;  // the total CV1 bend steps
int voice_bend2_interval;  // the total CV2 bend steps

#define NOTE_MONO_MAX 8
#define NOTE_POLY_MAX 16

#define VOICE_RETRIG_START 2
#define VOICE_RETRIG_STOP 1
#define VOICE_RETRIG_IDLE 0

// state
unsigned char damper1;  // 1 = down, 0 = up
unsigned char damper2;  // 1 = down, 0 = up
unsigned char voice1_cur;  // the current note on the CV1 DAC
unsigned char voice2_cur;  // the current note on the CV2 DAC
unsigned char voice1_retrig;  // 2 = start retrig, 1 = stop retrig, 0 = idle
unsigned char voice2_retrig;  // 2 = start retrig, 1 = stop retrig, 0 = idle
unsigned char voice1_retrig_note;  // the note to retrig
unsigned char voice2_retrig_note;  // the note to retrig
int voice1_transpose;  // transpose up or down
int voice2_transpose;  // transpose up or down
// single and split mode
unsigned char voice1_mono_prio[NOTE_MONO_MAX];
unsigned char voice2_mono_prio[NOTE_MONO_MAX];
unsigned char voice1_mono_pos;
unsigned char voice2_mono_pos;
unsigned char voice1_mono_keypressed;
unsigned char voice2_mono_keypressed;
unsigned char voice1_playing;
unsigned char voice2_playing;
// poly mode
unsigned char note_poly_slots[NOTE_POLY_MAX];
unsigned char note_poly_hold[NOTE_POLY_MAX];

// function prototypes
void voice_arp_note_on(unsigned char note);
void voice_arp_note_off(unsigned char note);
void voice_poly_note_on(unsigned char note);
void voice_poly_note_off(unsigned char note);
void voice_mono_note_on(unsigned char voice, unsigned char note);
void voice_mono_note_off(unsigned char voice, unsigned char note);
void voice_output_ctrl(unsigned char voice, unsigned char note, unsigned char on);

// init the voice manager code
void voice_init(void) {
	// load the config from flash memory
//	voice_set_mode(config_store_get_val(CONFIG_VOICE_MODE),//			config_store_get_val(CONFIG_VOICE_SPLIT));//	voice_set_unit(config_store_get_val(CONFIG_VOICE_UNIT));//	voice_set_pitch_bend_range(0, config_store_get_val(CONFIG_VOICE_BEND1));//	voice_set_pitch_bend_range(1, config_store_get_val(CONFIG_VOICE_BEND2));

	// fixed settings
	voice_set_mode(VOICE_MODE_SINGLE, 60);
	voice_set_unit(0);
	voice_set_pitch_bend_range(0, 2);
	voice_set_pitch_bend_range(1, 2);

	// reset everything
	voice1_cur = 60;  // make sure we reset to this note
	voice2_cur = 60;  // make sure we reset to this note
	voice_state_reset();
}

// runs the timer task
void voice_timer_task(void) {
	// voice 1 retrig
	if(voice1_retrig == VOICE_RETRIG_START) {
		voice1_retrig_note = voice1_cur;
		voice_output_ctrl(0, voice1_retrig_note, 0);
		voice1_retrig = VOICE_RETRIG_STOP;
	}
	else if(voice1_retrig == VOICE_RETRIG_STOP) {
		voice_output_ctrl(0, voice1_retrig_note, 1);
		voice1_retrig = VOICE_RETRIG_IDLE;
	}

	// voice 2 retrig
	if(voice2_retrig == VOICE_RETRIG_START) {
		voice2_retrig_note = voice2_cur;
		voice_output_ctrl(1, voice2_retrig_note, 0);
		voice2_retrig = VOICE_RETRIG_STOP;
	}
	else if(voice2_retrig == VOICE_RETRIG_STOP) {
		voice_output_ctrl(1, voice2_retrig_note, 1);
		voice2_retrig = VOICE_RETRIG_IDLE;
	}
}

// sets up a voice mode
void voice_set_mode(unsigned char mode, unsigned char split) {
	if(mode == VOICE_MODE_SPLIT) {
		voice_mode = mode;
		voice_split = split & 0x7f;
	}
	else if(mode == VOICE_MODE_POLY) {
		voice_mode = mode;
		voice_split = 0;
	}
	else if(mode == VOICE_MODE_ARP) {
		voice_mode = mode;
		voice_split = 0;
	}
	else if(mode == VOICE_MODE_VELO) {
		voice_mode = mode;
		voice_split = 0;
	}
	else {
		voice_mode = VOICE_MODE_SINGLE;
		voice_split = 0;
	}
//	// store settings to flash memory
//	config_store_set_val(CONFIG_VOICE_MODE, voice_mode);
//	config_store_set_val(CONFIG_VOICE_SPLIT, voice_split);
	voice_state_reset();
}

// set up the voice unit
void voice_set_unit(unsigned char unit) {
	voice_unit = unit;
	if(voice_unit > 7) voice_unit = 7;
//	// store settings to flash memory
//	config_store_set_val(CONFIG_VOICE_UNIT, voice_unit);
	voice_state_reset();
}

// set the pitch bend up amount
void voice_set_pitch_bend_range(unsigned char voice, unsigned char bend) {
	unsigned char bend_semi = bend;
 	if(bend_semi > 12) bend_semi = 12;
	else if(bend_semi < 1) bend_semi = 1;
	if(voice) {
//		// store settings to flash memory
//		config_store_set_val(CONFIG_VOICE_BEND2, bend_semi);
		voice_bend2_interval = 8192 / (bend_semi * NOTE_STEP_SIZE);
	}
	else {
//		// store settings to flash memory
//		config_store_set_val(CONFIG_VOICE_BEND1, bend_semi);
		voice_bend1_interval = 8192 / (bend_semi * NOTE_STEP_SIZE);
	}
}

// set the note offset (transpose) for generating pitches
void voice_set_transpose(unsigned char voice, int transpose) {
	if(voice) {
		voice2_transpose = transpose;
	}
	else {
		voice1_transpose = transpose;
	}
}

// note on
void voice_note_on(unsigned char voice, unsigned char note, unsigned char velocity) {
	// ignore unsupported notes
	if(note < NOTE_LOW || note > NOTE_HIGH) return;

	// single mode
	if(voice_mode == VOICE_MODE_SINGLE) {
		voice_mono_note_on(voice, note);
	}
	// split keyboard mode
	else if(voice_mode == VOICE_MODE_SPLIT) {
		if(voice) return;  // only voice 0 matters
		if(note < voice_split) voice_mono_note_on(0, note);
		else voice_mono_note_on(1, note);
	}
	// poly mode
	else if(voice_mode == VOICE_MODE_POLY) {
		if(voice) return; // only voice 0 matters
		voice_poly_note_on(note);
	}
	// arp mode
	else if(voice_mode == VOICE_MODE_ARP) {
		if(voice) return;  // only voice 0 matters
		voice_arp_note_on(note);
	}
	// velocity mode
	else if(voice_mode == VOICE_MODE_VELO) {
		if(voice) return;  // only voice 0 matters
		voice_mono_note_on(0, note);  // use single mode on voice 0
		cv_gate_ctrl_cv(1, velocity << 5);  // velocity goes to CV 2
		cv_gate_ctrl_gate(1, 0);  // force gate off
	}
}

// note off
void voice_note_off(unsigned char voice, unsigned char note) {
	// ignore unsupported notes
	if(note < NOTE_LOW || note > NOTE_HIGH) return;

	// single mode
	if(voice_mode == VOICE_MODE_SINGLE) {
		voice_mono_note_off(voice, note);
	}
	// split keyboard mode
	else if(voice_mode == VOICE_MODE_SPLIT) {
		if(note < voice_split) voice_mono_note_off(0, note);
		else voice_mono_note_off(1, note);
	}
	// poly mode
	else if(voice_mode == VOICE_MODE_POLY) {
		if(voice != 0) return;	// only voice 0 matters
		voice_poly_note_off(note);
	}
	// arp mode
	else if(voice_mode == VOICE_MODE_ARP) {		
		if(voice) return;  // only voice 0 matters
		voice_arp_note_off(note);
	}
	// velocity mode
	else if(voice_mode == VOICE_MODE_VELO) {
		if(voice) return;  // only voice 0 matters
		voice_mono_note_off(0, note);  // use single mode on voice 0
		// leave CV2 with the last velocity
	}
}

// turn all notes off and reset the prio list
void voice_all_notes_off(unsigned char voice) {
	unsigned char i;

    if(voice) {
    	// reset stuff
	    damper2 = 0;
	    voice2_retrig = VOICE_RETRIG_IDLE;
    
	    // clear note stuff
	    for(i = 0; i < NOTE_MONO_MAX; i ++) {
		    voice2_mono_prio[i] = 0;
    	}
	    voice2_mono_pos = 0;
	    voice2_mono_keypressed = 0;
	    voice2_playing = 0;

    	// force voices off and reset bends
	    cv_gate_ctrl_note(1, voice2_cur + voice2_transpose, 0);
	    cv_gate_ctrl_bend(1, 0);
    }
    else {
    	// reset stuff
    	damper1 = 0;
	    voice1_retrig = VOICE_RETRIG_IDLE;
    
	    // clear note stuff
	    for(i = 0; i < NOTE_MONO_MAX; i ++) {
		    voice1_mono_prio[i] = 0;
    	}
	    voice1_mono_pos = 0;
	    voice1_mono_keypressed = 0;
	    voice1_playing = 0;

    	// force voices off and reset bends
	    cv_gate_ctrl_note(0, voice1_cur + voice1_transpose, 0);
	    cv_gate_ctrl_bend(0, 0);
    }

    if(voice_mode == VOICE_MODE_POLY) {
	    for(i = 0; i < NOTE_POLY_MAX; i ++) {
		    note_poly_slots[i] = 0;
		    note_poly_hold[i] = 0;
	    }
    }
}

// damper pedal
void voice_damper(unsigned char voice, unsigned char state) {
	unsigned char i;

	// single mode
	if(voice_mode == VOICE_MODE_SINGLE || voice_mode == VOICE_MODE_VELO) {
		if(state) {
			if(voice) damper2 = 1;
			else damper1 = 1;
		}
		else {
			if(voice) {
				damper2 = 0;
				// note is still playing but no keys are held
				if(voice2_cur && !voice2_mono_keypressed) {
					voice_output_ctrl(1, voice2_cur, 0);
				}
			}
			else {
				damper1 = 0;
				// note is still playing but no keys are held
				if(voice1_cur && !voice1_mono_keypressed) {
					voice_output_ctrl(0, voice1_cur, 0);
				}
			}
		}
	}
	// split mode
	else if(voice_mode == VOICE_MODE_SPLIT) {
		if(voice) return;  // only voice 0 matters
		if(state) {
			damper1 = 1;
			damper2 = 1;
		}
		else {
			damper1 = 0;
			damper2 = 0;
			// note is still playing but no keys are held
			if(voice1_cur && !voice1_mono_keypressed) {
				voice1_mono_keypressed = 0;
				voice_output_ctrl(0, voice1_cur, 0);
			}
			// note is still playing but no keys are held
			if(voice2_cur && !voice2_mono_keypressed) {
				voice2_mono_keypressed = 0;
				voice_output_ctrl(1, voice2_cur, 0);
			}
		}
	}
	// poly mode
	else if(voice_mode == VOICE_MODE_POLY) {
		if(voice) return;  // only voice 0 matters
		if(state) {
			damper1 = 1;
			damper2 = 1;
		}
		else {
			damper1 = 0;
			damper2 = 0;
			// process all notes
			for(i = 0; i < NOTE_POLY_MAX; i ++) {
				// note is playing but not held
				if(note_poly_slots[i] && !note_poly_hold[i]) {
					if((i >> 1) == voice_unit) {
						voice_output_ctrl(i & 0x01, note_poly_slots[i], 0);
					}
					note_poly_slots[i] = 0;
				}
			}
		}
	}
	// arp mode
	else if(voice_mode == VOICE_MODE_ARP) {
		if(state) {
			damper1 = 1;
			damper2 = 1;
		}
		else {
			damper1 = 0;
			damper2 = 0;
			// notes are still playing
			if(voice1_cur || voice2_cur) {
				// no keys are held
				if(!voice1_mono_keypressed && !voice2_mono_keypressed) {
					voice_output_ctrl(0, voice1_cur, 0);
					voice_output_ctrl(1, voice2_cur, 0);
				}
				// only voice1 is held
				else if(voice1_mono_keypressed && !voice2_mono_keypressed) {
					voice_mono_note_on(1, voice1_cur);
				}
				// only voice2 is held
				else if(voice2_mono_keypressed && !voice1_mono_keypressed) {
					voice_mono_note_on(0, voice2_cur);
				}
			}
		}
	}
}

// pitch bend
void voice_pitch_bend(unsigned char voice, unsigned int bend) {
	int bend_amount = bend - 0x2000;  // range -8192 to +8191
	int temp;

	// poly, split or arp mode
	if(voice_mode == VOICE_MODE_POLY || 
			voice_mode == VOICE_MODE_SPLIT ||
			voice_mode == VOICE_MODE_ARP) {
		temp = bend_amount / voice_bend1_interval;
		cv_gate_ctrl_bend(0, temp);
		cv_gate_ctrl_bend(1, temp);
	}
	// single mode
	else if(voice_mode == VOICE_MODE_SINGLE || voice_mode == VOICE_MODE_VELO) {
		if(voice) {
			cv_gate_ctrl_bend(1, bend_amount / voice_bend2_interval);
		}
		else {
			cv_gate_ctrl_bend(0, bend_amount / voice_bend1_interval);
		}
	}
}

//
// PRIVATE FUNCTIONS
//
// handle arp mode note on
void voice_arp_note_on(unsigned char note) {
	// no voices are playing
	if(!voice1_playing) {
		// turn on both voices
		voice_mono_note_on(0, note);
		voice_mono_note_on(1, note);
	}
	// voice 1 is already playing
	else {
		// send new note only to voice 2
		voice_mono_note_on(1, note);
	}
}

// handle arp mode note off
void voice_arp_note_off(unsigned char note) {
	// if the damper is on
	// and voice1 and voice2 are playing
	// and voice1 and voice2 are playing different notes
	// remove the voice1 note from voice2 list
	if(damper1 && voice1_playing && voice2_playing && (voice1_cur != voice2_cur)) {
		voice_mono_note_off(1, voice1_cur);
	}

	// try turning off this note from both voices
	voice_mono_note_off(0, note);
	voice_mono_note_off(1, note);
	// if only voice 2 is now playing, make voice 1 play the same note
	if(!voice1_playing && voice2_playing) {
		voice_mono_note_on(0, voice2_cur);
	}
}

// handle poly mode note on
void voice_poly_note_on(unsigned char note) {
	unsigned char i;
	unsigned char slot = 255;
	unsigned char voice;
	unsigned char retrig = 0;

	// check if note is on or find a free slot
	for(i = 0; i < NOTE_POLY_MAX; i ++) {
		// note already playing - need to retrig
		if(note_poly_slots[i] == note) {
			retrig = 1;
			slot = i;
			break;
		}
		// slot is available
		else if(note_poly_slots[i] == 0) {
			slot = i;
			break;
		}
	}

	if(slot != 255) {
		// assign this slot
		note_poly_slots[slot] = note;
		note_poly_hold[slot] = note;
		// this slot is ours
		if((slot >> 1) == voice_unit) {
			voice = slot & 0x01;
			voice_output_ctrl(voice, note, 1);
			// we need to retrigger this voice
			if(retrig) {
				if(voice) voice2_retrig = VOICE_RETRIG_START;
				else voice1_retrig = VOICE_RETRIG_START;
			}
		}
	}
}

// handle poly mode note off
void voice_poly_note_off(unsigned char note) {
	unsigned char i;
	unsigned char slot = 255;
	// check if note is on or find a free slot
	for(i = 0; i < NOTE_POLY_MAX; i ++) {
		// note is playing - mark this slot and stop searching
		if(note_poly_slots[i] == note) {
			slot = i;
			break;
		}
	}
	// found this note playing
	if(slot != 255) {
		// this note is no longer held
		note_poly_hold[slot] = 0;
		// unassign the slot / turn of the note - if the damper is up
		if(!damper1) {
			note_poly_slots[slot] = 0;
			// this slot is ours
			if((slot >> 1) == voice_unit) {
				voice_output_ctrl((slot & 0x01), note, 0);
			}
		}
	}
}

// handle mono note on
void voice_mono_note_on(unsigned char voice, unsigned char note) {
	unsigned char i;
	if(voice) {
		// clear this note if it already exists
		for(i = 0; i < NOTE_MONO_MAX; i ++) {
			if(voice2_mono_prio[i] == note) {
				voice2_mono_prio[i] = 0;
			}
		}
		if(voice2_cur == note) voice2_retrig = VOICE_RETRIG_START;
		voice2_mono_pos = (voice2_mono_pos + 1) & 0x07;
		voice2_mono_prio[voice2_mono_pos] = note;
		voice_output_ctrl(1, note, 1);
		voice2_mono_keypressed = 1;	
	}
	else {
		// clear this note if it already exists
		for(i = 0; i < NOTE_MONO_MAX; i ++) {
			if(voice1_mono_prio[i] == note) {
				voice1_mono_prio[i] = 0;
			}
		}
		if(voice1_cur == note) voice1_retrig = VOICE_RETRIG_START;
		voice1_mono_pos = (voice1_mono_pos + 1) & 0x07;
		voice1_mono_prio[voice1_mono_pos] = note;
		voice_output_ctrl(0, note, 1);
		voice1_mono_keypressed = 1;
	}
}

// handle mono note off
void voice_mono_note_off(unsigned char voice, unsigned char note) {
	unsigned char i;
	if(voice) {
		// clear this note from the list
		for(i = 0; i < NOTE_MONO_MAX; i ++) {
			if(voice2_mono_prio[i] == note) {
				voice2_mono_prio[i] = 0;
			}
		}
		// if our current note is still active, return
		if(voice2_mono_prio[voice2_mono_pos]) return;
		// our current note is off, time to find a new note
		unsigned char rev_count = (voice2_mono_pos - 1) & 0x07;
		while(rev_count != voice2_mono_pos) {
			// we found another active note
			if(voice2_mono_prio[rev_count]) {
				voice_output_ctrl(1, voice2_mono_prio[rev_count], 1);
				voice2_mono_pos = rev_count;
				return;
			}
			rev_count = (rev_count - 1) & 0x07;
		}
		// no notes were found
		voice2_mono_keypressed = 0;
		if(!damper2) {
			voice_output_ctrl(1, voice2_cur, 0);
		}
	}
	else {
		// clear this note from the list
		for(i = 0; i < NOTE_MONO_MAX; i ++) {
			if(voice1_mono_prio[i] == note) {
				voice1_mono_prio[i] = 0;
			}
		}
		// if our current note is still active, return
		if(voice1_mono_prio[voice1_mono_pos]) return;
		// our current note is off, time to find a new note
		unsigned char rev_count = (voice1_mono_pos - 1) & 0x07;
		while(rev_count != voice1_mono_pos) {
			// we found another active note
			if(voice1_mono_prio[rev_count]) {
				voice_output_ctrl(0, voice1_mono_prio[rev_count], 1);
				voice1_mono_pos = rev_count;
				return;
			}
			rev_count = (rev_count - 1) & 0x07;
		}
		// no notes were found
		voice1_mono_keypressed = 0;
		if(!damper1) {
			voice_output_ctrl(0, voice1_cur, 0);
		}
	}
}

// voice output control
void voice_output_ctrl(unsigned char voice, unsigned char note, unsigned char on) {
	if(voice == 1) {
		voice2_cur = note;
		if(on) {
			cv_gate_ctrl_note(1, voice2_cur + voice2_transpose, 1);
			voice2_playing = 1;
		}
		else {
			cv_gate_ctrl_note(1, voice2_cur + voice2_transpose, 0);
			voice2_playing = 0;
		}
	}
	else {
		voice1_cur = note;
		if(on) {
			cv_gate_ctrl_note(0, voice1_cur + voice1_transpose, 1);
			voice1_playing = 1;
		}
		else {
			cv_gate_ctrl_note(0, voice1_cur + voice1_transpose, 0);
			voice1_playing = 0;
		}
	}
}

// reset all voice state 
void voice_state_reset(void) {
	unsigned char i;
	// reset stuff
	damper1 = 0;
	damper2 = 0;
	voice1_retrig = VOICE_RETRIG_IDLE;
	voice2_retrig = VOICE_RETRIG_IDLE;

	// clear note stuff
	for(i = 0; i < NOTE_MONO_MAX; i ++) {
		voice1_mono_prio[i] = 0;
		voice2_mono_prio[i] = 0;
	}
	voice1_mono_pos = 0;
	voice2_mono_pos = 0;
	voice1_mono_keypressed = 0;
	voice2_mono_keypressed = 0;
	for(i = 0; i < NOTE_POLY_MAX; i ++) {
		note_poly_slots[i] = 0;
		note_poly_hold[i] = 0;
	}
	voice1_playing = 0;
	voice2_playing = 0;

	// force voices off and reset bends
	voice1_transpose = 0;
	voice2_transpose = 0;
	cv_gate_ctrl_note(0, voice1_cur + voice1_transpose, 0);
	cv_gate_ctrl_note(1, voice2_cur + voice2_transpose, 0);
	voice1_cur = 0;
	voice2_cur = 0;
	cv_gate_ctrl_bend(0, 0);
	cv_gate_ctrl_bend(1, 0);
}
