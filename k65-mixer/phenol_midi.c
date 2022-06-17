/*
 * K65 Phenol Mixer - MIDI Handler
 *
 * Copyright 2014: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 *
 */
#include "phenol_midi.h"
#include "midi_callbacks.h"
#include "midi.h"
#include "usb_ctrl.h"
#include "TimeDelay.h"
#include "voice.h"
#include "cv_gate_ctrl.h"
#include "ioctl.h"
#include "switch_filter.h"
#include "pulse_div.h"
#include "seq.h"
#include "midi_clock.h"

// device restart
#define BOOTLOADER_ADDR 0x9FC00000  // check this
#define DEV_ID 0x48

// LED blink times
#define MIDI_LED_BLINK_TIME 20

// trigger threshold for pitch bend gate trigger#define PITCH_BEND_TRIG_UP 0x27ff#define PITCH_BEND_TRIG_DOWN 0x17ff
// event mappings#define EVENT_MAP_UNASSIGNED 0#define EVENT_MAP_NOTE 1#define EVENT_MAP_CC 2#define EVENT_MAP_PITCH_BEND 3
// CV/gateunsigned char cv1_map;  // CV1 event mappingunsigned char cv2_map;  // CV2 event mappingunsigned char cv1_chan;  // CV1 receive channelunsigned char cv2_chan;  // CV2 receive channelunsigned char cv1_val;  // CV1 CC assignment / bend dirunsigned char cv2_val;  // CV2 CC assignment / bend dir
// direct control modeunsigned int cv_direct1;  // CV1 direct control mode valueunsigned int cv_direct2;  // CV2 direct control mode value

// stop ignore
unsigned char stop_ignore;  // 0 = disabled, 1 = enabled

// local functionsvoid phenol_midi_set_cv(unsigned char num, unsigned char map, unsigned char chan, unsigned char val);

// initialize the MIDI handler
void phenol_midi_init(void) {
	midi_init(DEV_ID);  // K65 device code
    phenol_midi_reset();
}

// reset any modes that were set (for soft power on)
void phenol_midi_reset(void) {
	// fixed settings
	phenol_midi_set_cv(0, EVENT_MAP_NOTE, 0, 0);
	phenol_midi_set_cv(1, EVENT_MAP_CC, 0, 1);
    phenol_midi_set_stop_ignore(0);  // disabled
}

// run the timer task to do mode switching
void phenol_midi_timer_task(void) {
	int sw;
	int val;

	sw = switch_filter_get_event();
	// got an event
	if(sw) {
		val = sw & 0xf000;
		sw = (sw & 0x0fff);
		// key down events
		if(val == SW_CHANGE_PRESSED) {			switch(sw) {
				case SW_REC:
					seq_rec_button(1);
					break;
				case SW_PLAY:
					seq_play_button(1);
					break;
			}
		}
		// key up events
		else if(val == SW_CHANGE_UNPRESSED) {
			switch(sw) {
				case SW_REC:
					seq_rec_button(0);
					break;
				case SW_PLAY:
					seq_play_button(0);
					break;
			}			
		}
	}
}

// get the state of the stop ignore mode
int phenol_midi_get_stop_ignore(void) {
    return stop_ignore;
}

// set the state of the stop ignore mode
void phenol_midi_set_stop_ignore(int state) {
    if(state) {
        stop_ignore = 1;
    }
    else {
        stop_ignore = 0;
    }
}

//
// ACTIVITY INDICATORS
//
// MIDI send activity - for blinking LED
void _midi_send_act(unsigned char port) {
	// not used
}

// MIDI receive activity - for blinking LED
void _midi_receive_act(unsigned char port) {
	ioctl_set_midi_in_led(MIDI_LED_BLINK_TIME);
}

//
// CHANNEL MESSAGES
//
// note off - note on with velocity = 0 calls this
void _midi_rx_note_off(unsigned char port,
		unsigned char channel, 
		unsigned char note) {
	if(channel == 15) return;  // channel 16 is not used for notes

	// sequencer note off
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel) {
		seq_midi_note_off(note);
	}

/*
	// CV/gate - note off
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel) {
		voice_note_off(0, note);
	}
	if(cv2_map == EVENT_MAP_NOTE && cv2_chan == channel) {
		voice_note_off(1, note);
	}
*/
}

// note on - note on with velocity > 0 calls this
void _midi_rx_note_on(unsigned char port,
		unsigned char channel, 
		unsigned char note, 
		unsigned char velocity) {
	if(channel == 15) return;  // channel 16 is not used for notes

	// sequencer note on
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel) {
		seq_midi_note_on(note);
	}
/*
	// note CV/gate
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel) {
		voice_note_on(0, note, velocity);
	}
	if(cv2_map == EVENT_MAP_NOTE && cv2_chan == channel) {
		voice_note_on(1, note, velocity);
	}
*/
}

// key pressure
void _midi_rx_key_pressure(unsigned char port,
		unsigned char channel, 
		unsigned char note,
		unsigned char pressure) {
	// not supported
}

// control change - called for controllers 0-119 only
void _midi_rx_control_change(unsigned char port,
		unsigned char channel,
		unsigned char controller,
		unsigned char value) {

    // only care about our channel
    if(cv1_chan == channel) {
    	// sequencer mod wheel
	    if(controller == 1) {
			cv_gate_ctrl_cv(1, value << 5);
	    }
        // sequencer sustain pedal
        else if(controller == 64) {
            seq_midi_sustain_pedal(value);
        }
        // MIDI clock speed control
        else if(controller == 16) {
            // internal clock - adjust tempo
            if(midi_clock_get_internal()) {
                midi_clock_set_tempo((value << 1) + 40);
            }
            // external clock - adjust clock div
            else {
                midi_clock_set_clock_div(value >> 5);  // 0-3
            }
        }
    }
    

/*
	// CC damper pedal mode
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel && controller == 64) {
		voice_damper(0, value);
	}
	if(cv2_map == EVENT_MAP_NOTE && cv2_chan == channel && controller == 64) {
		voice_damper(1, value);
	}
	// direct control of CV2 - mod wheel
	if(cv1_chan == channel && controller == 1) {
		cv_gate_ctrl_cv(1, value << 5);
	}
	// CC CV/gate
	if(cv1_map == EVENT_MAP_CC && cv1_chan == channel && cv1_val == controller) {
		cv_gate_ctrl_cv(0, value << 5);
		// value 64-127
		if(value & 0x40) {
			cv_gate_ctrl_gate(0, 1);
		}
		// value 0-63
		else {
			cv_gate_ctrl_gate(0, 0);
		}
	}
	if(cv2_map == EVENT_MAP_CC && cv2_chan == channel && cv2_val == controller) {
		cv_gate_ctrl_cv(1, value << 5);
		// value 64-127
		if(value & 0x40) {
			cv_gate_ctrl_gate(1, 1);
		}
		// value 0-63
		else {
			cv_gate_ctrl_gate(1, 0);
		}
	}
*/
	// CC channel 16 direct control mode
//	if(channel == 15) {
	else if(channel == 15) {
		// CV1 value - MSB
		if(controller == 16) {
			cv_direct1 = (cv_direct1 & 0x1f) | (value << 5);
			cv_gate_ctrl_cv(0, cv_direct1);
		}
		// CV1 value - LSB
		else if(controller == 48) {
			cv_direct1 = (cv_direct1 & 0xfe0) | (value >> 2);
			cv_gate_ctrl_cv(0, cv_direct1);
		}
		// CV2 value - MSB
		else if(controller == 17) {
			cv_direct2 = (cv_direct2 & 0x1f) | (value << 5);
			cv_gate_ctrl_cv(1, cv_direct2);
		}
		// CV2 value - LSB
		else if(controller == 49) {
			cv_direct2 = (cv_direct2 & 0xfe0) | (value >> 2);
			cv_gate_ctrl_cv(1, cv_direct2);
		}
		// gate 1
		else if(controller == 18) {
			cv_gate_ctrl_gate(0, value >> 6);
		}
		// gate 2
		else if(controller == 19) {
			cv_gate_ctrl_gate(1, value >> 6);
		}
	}
}

// channel mode - all sounds off
void _midi_rx_all_sounds_off(unsigned char port,
		unsigned char channel) {
}

// channel mode - reset all controllers
void _midi_rx_reset_all_controllers(unsigned char port,
		unsigned char channel) {
}

// channel mode - local control
void _midi_rx_local_control(unsigned char port,
		unsigned char channel,
		unsigned char value) {
}

// channel mode - all notes off
void _midi_rx_all_notes_off(unsigned char port,
		unsigned char channel) {
}

// channel mode - omni off
void _midi_rx_omni_off(unsigned char port,
		unsigned char channel) {
}

// channel mode - omni on
void _midi_rx_omni_on(unsigned char port,
		unsigned char channel) {
}

// channel mode - mono on
void _midi_rx_mono_on(unsigned char port,
		unsigned char channel) {
}

// channel mode - poly on
void _midi_rx_poly_on(unsigned char port,
		unsigned char channel) {
}

// program change
void _midi_rx_program_change(unsigned char port,
		unsigned char channel,
		unsigned char program) {
	// set pitch bend range - only respond if the channels assigned to one of our channels
	if(cv1_chan == channel || cv2_chan == channel) {
		// program 0-11 = CV1 bend 1-12
		if(program < 12) {
			voice_set_pitch_bend_range(0, program + 1);
		}
		// program 12-23 = CV2 bend 1-12
		else if(program > 11 && program < 24) {
			voice_set_pitch_bend_range(1, program - 11);
		}
	}
}

// channel pressure
void _midi_rx_channel_pressure(unsigned char port,
		unsigned char channel,
		unsigned char pressure) {
}

// pitch bend
void _midi_rx_pitch_bend(unsigned char port,
		unsigned char channel,
		int bend) {

	// sequencer pitch bend
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel) {
		seq_midi_pitch_bend(bend);
	}

/*
	// pitch bend - note bend mode
	if(cv1_map == EVENT_MAP_NOTE && cv1_chan == channel) {
		voice_pitch_bend(0, bend);
	}
	if(cv2_map == EVENT_MAP_NOTE && cv2_chan == channel) {
		voice_pitch_bend(1, bend);
	}
*/
}

//
// SYSTEM COMMON MESSAGES
//
// song position
void _midi_rx_song_position(unsigned char port,
		unsigned int pos) {
    midi_clock_rx_song_position(pos);
}

// song select
void _midi_rx_song_select(unsigned char port,
		unsigned char song) {
	// unused
}

//
// SYSEX MESSAGES
//
// sysex message received
void _midi_rx_sysex_msg(unsigned char port,
		unsigned char data[], 
		unsigned char len) {
	// XXX setup message
}

//
// SYSTEM REALTIME MESSAGES
//
// timing tick
void _midi_rx_timing_tick(unsigned char port) {
    midi_clock_rx_timing_tick();
}

// start song
void _midi_rx_start_song(unsigned char port) {
    midi_clock_rx_start_song();
}

// continue song
void _midi_rx_continue_song(unsigned char port) {
    midi_clock_rx_continue_song();
}

// stop song
void _midi_rx_stop_song(unsigned char port) {
    if(stop_ignore) {
        return;
    }
    midi_clock_rx_stop_song();
}

// active sensing
void _midi_rx_active_sensing(unsigned char port) {
}

// system reset
void _midi_rx_system_reset(unsigned char port) {
}

//
// SPECIAL CALLBACKS
//
void _midi_restart_device(void) {
	DelayMs(100);
	void (*fptr)(void);
	fptr = (void (*)(void))BOOTLOADER_ADDR;
	fptr();
}

//
// CONFIG
//
// set a CV config
void phenol_midi_set_cv(unsigned char num, unsigned char map, 
		unsigned char chan, unsigned char val) {
	if(num > 1) return;
	if(num == 1) {
		cv2_map = map & 0x03;
		cv2_chan = chan & 0xff;
		cv2_val = val & 0x7f;
	}
	else {
		cv1_map = map & 0x03;
		cv1_chan = chan & 0xff;
		cv1_val = val & 0x7f;
	}
}
