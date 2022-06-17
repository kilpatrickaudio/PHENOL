/*
 * Multiport MIDI Receiver/Parser/Transmitter
 *
 * Copyright 2011: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include <plib.h>
#include "midi.h"
#include "midi_callbacks.h"

//#define MIDI_KA_SYSEX_HANDLER  // parse global KA SYSEX messages
// should we call the MIDI TX and RX activity callbacks?
//#define MIDI_TX_ACT
#define MIDI_RX_ACT
// size of received SYSEX message - this can be 1-254 
#define SYSEX_RX_BUFSIZE 200
// TX and RX bufs must be size is a power of 2
#define MIDI_RX_BUFSIZE 256
#define MIDI_TX_BUFSIZE 256

// sysex commands - these are Kilpatrick Audio global messages
unsigned char dev_type;
#define CMD_DEVICE_TYPE_QUERY 0x7c
#define CMD_DEVICE_TYPE_RESPONSE 0x7d
#define CMD_RESTART_DEVICE 0x7e

// channel mode messages
#define MIDI_CHANNEL_MODE_ALL_SOUNDS_OFF 120
#define MIDI_CHANNEL_MODE_RESET_ALL_CONTROLLERS 121
#define MIDI_CHANNEL_MODE_LOCAL_CONTROL 122
#define MIDI_CHANNEL_MODE_ALL_NOTES_OFF 123
#define MIDI_CHANNEL_MODE_OMNI_OFF 124
#define MIDI_CHANNEL_MODE_OMNI_ON 125
#define MIDI_CHANNEL_MODE_MONO_ON 126
#define MIDI_CHANNEL_MODE_POLY_ON 127

// state
#define RX_STATE_IDLE 0
#define RX_STATE_DATA0 1
#define RX_STATE_DATA1 2
#define RX_STATE_SYSEX_DATA 3
unsigned char midi_rx_state[MIDI_NUMPORTS];  // receiver state

// RX message
unsigned char midi_midi_rx_status_chan[MIDI_NUMPORTS];  // current message channel
unsigned char midi_rx_status[MIDI_NUMPORTS];  // current status byte
unsigned char midi_rx_data0[MIDI_NUMPORTS];  // data0 byte
unsigned char midi_rx_data1[MIDI_NUMPORTS];  // data1 byte

// RX buffer
unsigned char midi_rx_msg[MIDI_NUMPORTS][MIDI_RX_BUFSIZE];  // receive msg buffer
unsigned char midi_rx_in_pos[MIDI_NUMPORTS];
unsigned char midi_rx_out_pos[MIDI_NUMPORTS];
#define MIDI_RX_BUF_MASK (MIDI_RX_BUFSIZE - 1)

// TX message
unsigned char midi_tx_msg[MIDI_NUMPORTS][MIDI_TX_BUFSIZE];  // transmit msg buffer
unsigned char midi_tx_in_pos[MIDI_NUMPORTS];
unsigned char midi_tx_out_pos[MIDI_NUMPORTS];
#define MIDI_TX_BUF_MASK (MIDI_TX_BUFSIZE - 1)

// sysex buffer
unsigned char midi_sysex_rx_buf[MIDI_NUMPORTS][SYSEX_RX_BUFSIZE];
unsigned char midi_sysex_rx_buf_count[MIDI_NUMPORTS];

// local functions
void midi_process_msg(unsigned char port);
void midi_sysex_start(unsigned char port);
void midi_sysex_data(unsigned char port, unsigned char data);
void midi_sysex_end(unsigned char port);
void midi_sysex_parse_msg(unsigned char port);
void midi_control_change_parse_msg(unsigned char port,
		unsigned char channel, 
		unsigned char controller, 
		unsigned char value);
void midi_tx_add_byte(unsigned char port, unsigned char data);

// init the MIDI receiver module
void midi_init(unsigned char device_type) {
	int i;
	dev_type = device_type;
	for(i = 0; i < MIDI_NUMPORTS; i ++) {
		midi_rx_state[i] = RX_STATE_IDLE;
		midi_rx_status[i] = 255;  // no running status yet
 		midi_midi_rx_status_chan[i] = 0;
		midi_tx_in_pos[i] = 0;
		midi_tx_out_pos[i] = 0;
		midi_rx_in_pos[i] = 0;
		midi_rx_out_pos[i] = 0;
		midi_sysex_rx_buf_count[i] = 0;
	}
}

// returns 1 if there is data available to send on a port
unsigned char midi_tx_avail(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return 0;
	if(midi_tx_in_pos[port] == midi_tx_out_pos[port]) return 0;
	return 1;
}

// returns the next available TX byte for a port
unsigned char midi_tx_get_byte(unsigned char port) {
	unsigned char ret;
	if(port > (MIDI_NUMPORTS - 1)) return 0;
	ret = midi_tx_msg[port][midi_tx_out_pos[port]];
	midi_tx_out_pos[port] = (midi_tx_out_pos[port] + 1) & MIDI_TX_BUF_MASK;
	return ret;
}

// handle a new byte received from the stream
void midi_rx_byte(unsigned char port, unsigned char rx_byte) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_rx_msg[port][midi_rx_in_pos[port]] = rx_byte;
	midi_rx_in_pos[port] = (midi_rx_in_pos[port] + 1) & MIDI_RX_BUF_MASK;
}

// receive task - call this on a timer interrupt
// returns 0 if there is nothing to do
int midi_rx_task(unsigned char port) {
	unsigned char stat, chan;
	unsigned char rx_byte;
	if(port > (MIDI_NUMPORTS - 1)) return 0;

	// get data from RX buffer
	if(midi_rx_in_pos[port] == midi_rx_out_pos[port]) return 0;
	rx_byte = midi_rx_msg[port][midi_rx_out_pos[port]];
	midi_rx_out_pos[port] = (midi_rx_out_pos[port] + 1) & MIDI_RX_BUF_MASK;

	// status byte
	if(rx_byte & 0x80) {
		stat = (rx_byte & 0xf0);
	   	chan = (rx_byte & 0x0f);

		// system messages
   		if(stat == 0xf0) {
			// realtime messages - does not reset running status
	     	if(rx_byte == MIDI_TIMING_TICK) {
				_midi_rx_timing_tick(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
    	 	}
	     	else if(rx_byte == MIDI_START_SONG) {
				_midi_rx_start_song(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
    		}	
     		else if(rx_byte == MIDI_CONTINUE_SONG) {
				_midi_rx_continue_song(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
     		}
	     	else if(rx_byte == MIDI_STOP_SONG) {
				_midi_rx_stop_song(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
	     	}
			else if(rx_byte == MIDI_ACTIVE_SENSING) {
				_midi_rx_active_sensing(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
			}
			else if(rx_byte == MIDI_SYSTEM_RESET) {
				midi_midi_rx_status_chan[port] = 255;  // reset running status channel
				midi_rx_status[port] = 0;
				midi_rx_state[port] = RX_STATE_IDLE;
				_midi_rx_system_reset(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
			}
			else if(rx_byte == 0xf9 || rx_byte == 0xfd) {
				// undefined system realtime message
				return 1;
			}
			// sysex messages
    		else if(rx_byte == MIDI_SYSEX_START) {
				midi_sysex_start(port);
				midi_midi_rx_status_chan[port] = 255;  // reset running status channel
				midi_rx_status[port] = rx_byte;
				midi_rx_state[port] = RX_STATE_SYSEX_DATA;
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
	     	}
	     	else if(rx_byte == MIDI_SYSEX_END) {
				midi_sysex_end(port);
				midi_midi_rx_status_chan[port] = 255;  // reset running status channel
				midi_rx_status[port] = 0;
				midi_rx_state[port] = RX_STATE_IDLE;
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
				return 1;
     		}
			else {
				// are we currently receiving sysex?
				if(midi_rx_state[port] == RX_STATE_SYSEX_DATA) {
					midi_sysex_end(port);
#ifdef MIDI_RX_ACT
					_midi_receive_act(port);  // receive activity
#endif
				}
				// system common messages
    	 		if(rx_byte == MIDI_SONG_POSITION) {
					midi_midi_rx_status_chan[port] = 255;  // reset running status channel
					midi_rx_status[port] = rx_byte;
					midi_rx_state[port] = RX_STATE_DATA0;
					return 1;
    	 		}
	     		else if(rx_byte == MIDI_SONG_SELECT) {
					midi_midi_rx_status_chan[port] = 255;  // reset running status channel
					midi_rx_status[port] = rx_byte;
					midi_rx_state[port] = RX_STATE_DATA0;
					return 1;
    	 		}
				// undefined / unsupported system common messages
				// MTC quarter frame, 0xf4, 0xf5, tune request
				midi_midi_rx_status_chan[port] = 255;  // reset running status channel
				midi_rx_status[port] = 0;
				midi_rx_state[port] = RX_STATE_IDLE;
				return 1;
			}
   		}
	   	// channel messages
   		else {
			// do we have a sysex message current receiving?
			if(midi_rx_state[port] == RX_STATE_SYSEX_DATA) {
				midi_sysex_end(port);
#ifdef MIDI_RX_ACT
				_midi_receive_act(port);  // receive activity
#endif
			}
			// channel status
     		if(stat == MIDI_NOTE_OFF) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
	     	}
    	 	if(stat == MIDI_NOTE_ON) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
     		}
	     	if(stat == MIDI_KEY_PRESSURE) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
    	 	}
	    	if(stat == MIDI_CONTROL_CHANGE) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
	     	}
    	 	if(stat == MIDI_PROG_CHANGE) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
	     	}
    	 	if(stat == MIDI_CHAN_PRESSURE) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
	     	}
    	 	if(stat == MIDI_PITCH_BEND) {
				midi_midi_rx_status_chan[port] = chan;
				midi_rx_status[port] = stat;
				midi_rx_state[port] = RX_STATE_DATA0;
				return 1;
	    	}
   		}
   		return 1;  // just in case
	}
 	// data byte 0
 	if(midi_rx_state[port] == RX_STATE_DATA0) {
   		midi_rx_data0[port] = rx_byte;

		// data length = 1 - process these messages right away
   		if(midi_rx_status[port] == MIDI_SONG_SELECT ||
				midi_rx_status[port] == MIDI_PROG_CHANGE ||
      			midi_rx_status[port] == MIDI_CHAN_PRESSURE) {
     		midi_process_msg(port);
			// if this message supports running status
     		if(midi_midi_rx_status_chan[port] != 255) {
				midi_rx_state[port] = RX_STATE_DATA0;  // loop back for running status
			}
			else {
				midi_rx_state[port] = RX_STATE_IDLE;
			}
   		}
   		// data length = 2
   		else {
     		midi_rx_state[port] = RX_STATE_DATA1;
   		}
	   return 1;
 	}

 	// data byte 1
 	if(midi_rx_state[port] == RX_STATE_DATA1) {
   		midi_rx_data1[port] = rx_byte;
   		midi_process_msg(port);
		// if this message supports running status
   		if(midi_midi_rx_status_chan[port] != 255) {   		
   			midi_rx_state[port] = RX_STATE_DATA0;  // loop back for running status
		}
		else {
			midi_rx_state[port] = RX_STATE_IDLE;
		}
   		return 1;
 	}

	// sysex data
	if(midi_rx_state[port] == RX_STATE_SYSEX_DATA) {
		midi_sysex_data(port, rx_byte);
		return 1;
	}
    return 1;
}

//
// local functions
//
// process a received message
void midi_process_msg(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
#ifdef MIDI_RX_ACT
	_midi_receive_act(port);  // receive activity
#endif
	if(midi_rx_status[port] == MIDI_SONG_POSITION) {
   		_midi_rx_song_position(port, (midi_rx_data1[port] << 7) | midi_rx_data0[port]);
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_SONG_SELECT) {
   		_midi_rx_song_select(port, midi_rx_data0[port]);
   		return;
 	}
	// this is a channel message by this point
 	if(midi_rx_status[port] == MIDI_NOTE_OFF) {
   		_midi_rx_note_off(port, midi_midi_rx_status_chan[port], midi_rx_data0[port]);
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_NOTE_ON) {
   		if(midi_rx_data1[port] == 0) {
			_midi_rx_note_off(port, midi_midi_rx_status_chan[port], 
					midi_rx_data0[port]);
   		}
		else {
			_midi_rx_note_on(port, midi_midi_rx_status_chan[port], 
				midi_rx_data0[port], midi_rx_data1[port]);
		}
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_KEY_PRESSURE) {
   		_midi_rx_key_pressure(port, midi_midi_rx_status_chan[port], 
			midi_rx_data0[port], midi_rx_data1[port]);
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_CONTROL_CHANGE) {
		midi_control_change_parse_msg(port, midi_midi_rx_status_chan[port], 
			midi_rx_data0[port], midi_rx_data1[port]);
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_PROG_CHANGE) {
   		_midi_rx_program_change(port, midi_midi_rx_status_chan[port], 
			midi_rx_data0[port]);
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_CHAN_PRESSURE) {
   		_midi_rx_channel_pressure(port, midi_midi_rx_status_chan[port], 
			midi_rx_data0[port]);
   		return;
 	}
 	if(midi_rx_status[port] == MIDI_PITCH_BEND) {
   		_midi_rx_pitch_bend(port, midi_midi_rx_status_chan[port], 
			(int)((midi_rx_data1[port] << 7) | midi_rx_data0[port]));
   		return;
 	}
}

// handle sysex start
void midi_sysex_start(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_sysex_rx_buf_count[port] = 0;
}

// handle sysex data
void midi_sysex_data(unsigned char port, unsigned char data) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	if(midi_sysex_rx_buf_count[port] < SYSEX_RX_BUFSIZE) {
		midi_sysex_rx_buf[port][midi_sysex_rx_buf_count[port]] = data;
		midi_sysex_rx_buf_count[port] ++;
	}
}

// handle sysex end
void midi_sysex_end(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_sysex_parse_msg(port);
}

// parse a received sysex message for globally supported stuff
void midi_sysex_parse_msg(unsigned char port) {
// handle global KA messages internally
#ifdef MIDI_KA_SYSEX_HANDLER
	unsigned char cmd;
	if(sysex_rx_buf_count[port] < 4) return;
	// Kilpatrick Audio command
	if(sysex_rx_buf[port][0] == 0x00 &&
			sysex_rx_buf[port][1] == 0x01 &&
			sysex_rx_buf[port][2] == 0x72) {
		cmd = sysex_rx_buf[port][3];
		// device type query - global command
		if(cmd == CMD_DEVICE_TYPE_QUERY) {
			_midi_tx_sysex1(port, CMD_DEVICE_TYPE_RESPONSE, dev_type);
		}
		// restart device - global command with addressing
		else if(cmd == CMD_RESTART_DEVICE && sysex_rx_buf_count[port] == 9) {
			// check the device ID and special code ("KILL")
			if(sysex_rx_buf[port][4] == dev_type && 
					sysex_rx_buf[port][5] == 'K' &&
					sysex_rx_buf[port][6] == 'I' && 
					sysex_rx_buf[port][7] == 'L' &&
					sysex_rx_buf[port][8] == 'L') {
				_midi_restart_device();
			}
		}
		// non-global Kilpatrick Audio command - pass on to user code
		else {
			_midi_rx_sysex_msg(port, sysex_rx_buf[port], sysex_rx_buf_count[port]);
		}
	}
// pass all messages to the application
#else
	_midi_rx_sysex_msg(port, midi_sysex_rx_buf[port], midi_sysex_rx_buf_count[port]);
#endif
}

// parse a received controller change message
void midi_control_change_parse_msg(unsigned char port, 
		unsigned char channel, 
		unsigned char controller, 
		unsigned char value) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	// controllers
	if(controller < 120) {
		// pass through to user code
		_midi_rx_control_change(port, channel, controller, value);
	}
	// all sounds off
	else if(controller == MIDI_CHANNEL_MODE_ALL_SOUNDS_OFF) {
		_midi_rx_all_sounds_off(port, channel);
	}
	// reset all controllers
	else if(controller == MIDI_CHANNEL_MODE_RESET_ALL_CONTROLLERS) {
		_midi_rx_reset_all_controllers(port, channel);
	}
	// local control
	else if(controller == MIDI_CHANNEL_MODE_LOCAL_CONTROL) {
		_midi_rx_local_control(port, channel, value);
	}
	// all notes off
	else if(controller == MIDI_CHANNEL_MODE_ALL_NOTES_OFF) {
		_midi_rx_all_notes_off(port, channel);
	}
	// omni off
	else if(controller == MIDI_CHANNEL_MODE_OMNI_OFF) {
		_midi_rx_omni_off(port, channel);
	}
	// omni on
	else if(controller == MIDI_CHANNEL_MODE_OMNI_ON) {
		_midi_rx_omni_on(port, channel);
	}
	// mono on
	else if(controller == MIDI_CHANNEL_MODE_MONO_ON) {
		_midi_rx_mono_on(port, channel);
	}
	// poly on
	else if(controller == MIDI_CHANNEL_MODE_POLY_ON) {
		_midi_rx_poly_on(port, channel);
	}	
}

// add a byte to the transmit queue - local use only
void midi_tx_add_byte(unsigned char port, unsigned char data) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_msg[port][midi_tx_in_pos[port]] = data;
	midi_tx_in_pos[port] = (midi_tx_in_pos[port] + 1) & MIDI_TX_BUF_MASK;
}

// gets the device type configured in the MIDI library
unsigned char midi_get_device_type(void) {
	return dev_type;
}

//
// SENDERS
//
// send note off - sends note on with velocity 0
void _midi_tx_note_off(unsigned char port,
		unsigned char channel,
		unsigned char note) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_NOTE_ON | (channel & 0x0f));
 	midi_tx_add_byte(port, (note & 0x7f));
	midi_tx_add_byte(port, 0x00);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send note on
void _midi_tx_note_on(unsigned char port,
		unsigned char channel,
		unsigned char note,
		unsigned char velocity) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_NOTE_ON | (channel & 0x0f));
 	midi_tx_add_byte(port, (note & 0x7f));
 	midi_tx_add_byte(port, (velocity & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send key pressure
void _midi_tx_key_pressure(unsigned char port,
				unsigned char channel,
			   	unsigned char note,
			   	unsigned char pressure) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_KEY_PRESSURE | (channel & 0x0f));
 	midi_tx_add_byte(port, (note & 0x7f));
 	midi_tx_add_byte(port, (pressure & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send control change
void _midi_tx_control_change(unsigned char port,
		unsigned char channel,
		unsigned char controller,
		unsigned char value) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, (controller & 0x7f));
 	midi_tx_add_byte(port, (value & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}


// send channel mode - all sounds off
void _midi_tx_all_sounds_off(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_ALL_SOUNDS_OFF);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - reset all controllers
void _midi_tx_reset_all_controllers(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_RESET_ALL_CONTROLLERS);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - local control
void _midi_tx_local_control(unsigned char port,
		unsigned char channel, 
		unsigned char value) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_LOCAL_CONTROL);
 	midi_tx_add_byte(port, (value & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - all notes off
void _midi_tx_all_notes_off(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_ALL_NOTES_OFF);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - omni off
void _midi_tx_omni_off(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_OMNI_OFF);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - omni on
void _midi_tx_omni_on(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_OMNI_ON);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - mono on
void _midi_tx_mono_on(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_MONO_ON);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel mode - poly on
void _midi_tx_poly_on(unsigned char port,
		unsigned char channel) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTROL_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, MIDI_CHANNEL_MODE_POLY_ON);
 	midi_tx_add_byte(port, 0);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send program change
void _midi_tx_program_change(unsigned char port,
		unsigned char channel,
		unsigned char program) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_PROG_CHANGE | (channel & 0x0f));
 	midi_tx_add_byte(port, (program & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send channel pressure
void _midi_tx_channel_pressure(unsigned char port,
		unsigned char channel,
		unsigned char pressure) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CHAN_PRESSURE | (channel & 0x0f));
 	midi_tx_add_byte(port, (pressure & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send pitch bend
void _midi_tx_pitch_bend(unsigned char port,
		unsigned char channel,
		unsigned int bend) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_PITCH_BEND | (channel & 0x0f));
 	midi_tx_add_byte(port, (bend & 0x7f));
 	midi_tx_add_byte(port, (bend & 0x3f80) >> 7);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// sysex message start
void _midi_tx_sysex_start(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_SYSEX_START);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// sysex message data byte
void _midi_tx_sysex_data(unsigned char port, 
		unsigned char data_byte) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, data_byte & 0x7f);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// sysex message end
void _midi_tx_sysex_end(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_SYSEX_END);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send a sysex packet with CMD and DATA - default MMA ID and dev type
void _midi_tx_sysex1(unsigned char port,
		unsigned char cmd, 
		unsigned char data) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	_midi_tx_sysex_start(port);
	_midi_tx_sysex_data(port, 0x00);
	_midi_tx_sysex_data(port, 0x01);
	_midi_tx_sysex_data(port, 0x72);
	_midi_tx_sysex_data(port, dev_type);
	_midi_tx_sysex_data(port, cmd & 0x7f);
	_midi_tx_sysex_data(port, data & 0x7f);
	_midi_tx_sysex_end(port);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send a sysex packet with CMD and 2 DATA bytes - default MMA ID and dev type
void _midi_tx_sysex2(unsigned char port,
		unsigned char cmd, 
		unsigned char data0, 
		unsigned char data1) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	_midi_tx_sysex_start(port);
	_midi_tx_sysex_data(port, 0x00);
	_midi_tx_sysex_data(port, 0x01);
	_midi_tx_sysex_data(port, 0x72);
	_midi_tx_sysex_data(port, dev_type);
	_midi_tx_sysex_data(port, cmd & 0x7f);
	_midi_tx_sysex_data(port, data0 & 0x7f);
	_midi_tx_sysex_data(port, data1 & 0x7f);
	_midi_tx_sysex_end(port);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send a sysex message with some number of bytes - no default MMA ID and dev type
void _midi_tx_sysex_msg(unsigned char port, 
		unsigned char data[], 
		unsigned char len) {
	int i;
	if(port > (MIDI_NUMPORTS - 1)) return;
	_midi_tx_sysex_start(port);
	for(i = 0; i < len; i ++) {
		_midi_tx_sysex_data(port, data[i] & 0x7f);
	}
	_midi_tx_sysex_end(port);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send song position
void _midi_tx_song_position(unsigned char port,
		unsigned int position) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_SONG_POSITION);
	midi_tx_add_byte(port, (position & 0x7f));
	midi_tx_add_byte(port, (position & 0x3f80) >> 7);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send song select
void _midi_tx_song_select(unsigned char port,
		unsigned char song) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_SONG_SELECT);
	midi_tx_add_byte(port, (song & 0x7f));
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send timing tick
void _midi_tx_timing_tick(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
	midi_tx_add_byte(port, MIDI_TIMING_TICK);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send start song
void _midi_tx_start_song(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_START_SONG);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send continue song
void _midi_tx_continue_song(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_CONTINUE_SONG);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send stop song
void _midi_tx_stop_song(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_STOP_SONG);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send active sensing
void _midi_tx_active_sensing(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_ACTIVE_SENSING);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send system reset
void _midi_tx_system_reset(unsigned char port) {
	if(port > (MIDI_NUMPORTS - 1)) return;
 	midi_tx_add_byte(port, MIDI_SYSTEM_RESET);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}

// send a debug string
void _midi_tx_debug(unsigned char port, char *str) {
	char *p = str;
	if(port > (MIDI_NUMPORTS - 1)) return;
	_midi_tx_sysex_start(port);
	_midi_tx_sysex_data(port, 0x00);
	_midi_tx_sysex_data(port, 0x01);
	_midi_tx_sysex_data(port, 0x72);
	_midi_tx_sysex_data(port, 0x01);
	while(*p) {
		_midi_tx_sysex_data(port, (*p) & 0x7f);
		p++;
	}
	_midi_tx_sysex_end(port);
#ifdef MIDI_TX_ACT
	_midi_send_act(port);  // send activity
#endif
}
