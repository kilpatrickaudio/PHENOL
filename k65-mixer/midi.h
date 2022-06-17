/*
 * MIDI Receiver/Parser/Transmitter Code - ver. 1.1
 *
 * Copyright 2011: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef MIDI_H
#define MIDI_H

// MIDI ports setting
// this is how many FIFOs will be created to deal with byte streams
// in and out of the system
#define MIDI_NUMPORTS 2

//
// MIDI status bytes
//
// channel messages
#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON 0x90
#define MIDI_KEY_PRESSURE 0xa0
#define MIDI_CONTROL_CHANGE 0xb0
#define MIDI_PROG_CHANGE 0xc0
#define MIDI_CHAN_PRESSURE 0xd0
#define MIDI_PITCH_BEND 0xe0
// system common messages
//#define MIDI_MTC_QFRAME 0xf1 - not supported
#define MIDI_SONG_POSITION 0xf2
#define MIDI_SONG_SELECT 0xf3
//#define MIDI_TUNE_REQUEST 0xf6 - not supported
// sysex exclusive messages
#define MIDI_SYSEX_START 0xf0
#define MIDI_SYSEX_END 0xf7
// system realtime messages
#define MIDI_TIMING_TICK 0xf8
#define MIDI_START_SONG 0xfa
#define MIDI_CONTINUE_SONG 0xfb
#define MIDI_STOP_SONG 0xfc
#define MIDI_ACTIVE_SENSING 0xfe
#define MIDI_SYSTEM_RESET 0xff

// init the MIDI receiver module
void midi_init(unsigned char device_type);

// returns 1 if there is data available to send on a port
unsigned char midi_tx_avail(unsigned char port);

// returns the next available TX byte for a port
unsigned char midi_tx_get_byte(unsigned char port);

// handle a new byte received from the stream
void midi_rx_byte(unsigned char port, unsigned char rx_byte);

// receive task - call this on a timer interrupt
// returns 0 if there is nothing to do
int midi_rx_task(unsigned char port);

// sets the learn mode - 1 = on, 0 = off
void midi_set_learn_mode(unsigned char mode);

// gets the device type configured in the MIDI library
unsigned char midi_get_device_type(void);

//
// SENDERS
//
// send note on
void _midi_tx_note_off(unsigned char port,
				unsigned char channel,
		       	unsigned char note);

// send note off - sends a note on with velocity 0
void _midi_tx_note_on(unsigned char port,
				unsigned char channel,
		      	unsigned char note,
		      	unsigned char velocity);			  
			   
// send key pressture
void _midi_tx_key_pressure(unsigned char port,
				unsigned char channel,
			   	unsigned char note,
			   	unsigned char pressure);

// send control change
void _midi_tx_control_change(unsigned char port,
		unsigned char channel,
		unsigned char controller,
		unsigned char value);

// send channel mode - all sounds off
void _midi_tx_all_sounds_off(unsigned char port, 
		unsigned char channel);

// send channel mode - reset all controllers
void _midi_tx_reset_all_controllers(unsigned char port, 
		unsigned char channel);

// send channel mode - local control
void _midi_tx_local_control(unsigned char port,
		unsigned char channel, 
		unsigned char value);

// send channel mode - all notes off
void _midi_tx_all_notes_off(unsigned char port,
		unsigned char channel);

// send channel mode - omni off
void _midi_tx_omni_off(unsigned char port,
		unsigned char channel);

// send channel mode - omni on
void _midi_tx_omni_on(unsigned char port,
		unsigned char channel);

// send channel mode - mono on
void _midi_tx_mono_on(unsigned char port,
		unsigned char channel);

// send channel mode - poly on
void _midi_tx_poly_on(unsigned char port,
		unsigned char channel);

// send program change
void _midi_tx_program_change(unsigned char port,
		unsigned char channel,
		unsigned char program);

// send channel pressure
void _midi_tx_channel_pressure(unsigned char port,
		unsigned char channel,
		unsigned char pressure);

// send pitch bend
void _midi_tx_pitch_bend(unsigned char port,
		unsigned char channel,
		unsigned int bend);
			 
// send sysex start
void _midi_tx_sysex_start(unsigned char port);

// send sysex data
void _midi_tx_sysex_data(unsigned char port,
		unsigned char data_byte);

// send sysex end
void _midi_tx_sysex_end(unsigned char port);

// send a sysex packet with CMD and DATA - default MMA ID and dev type
void _midi_tx_sysex1(unsigned char port,
		unsigned char cmd, 
		unsigned char data);

// send a sysex packet with CMD and 2 DATA bytes - default MMA ID and dev type
void _midi_tx_sysex2(unsigned char port,
		unsigned char cmd, 
		unsigned char data0, 
		unsigned char data1);

// send a sysex message with some number of bytes
void _midi_tx_sysex_msg(unsigned char port,
		unsigned char data[], 
		unsigned char len);

// send song position
void _midi_tx_song_position(unsigned char port,
		unsigned int position);

// send song select
void _midi_tx_song_select(unsigned char port,
		unsigned char song);

// send timing tick
void _midi_tx_timing_tick(unsigned char port);

// send start song
void _midi_tx_start_song(unsigned char port);

// send continue song
void _midi_tx_continue_song(unsigned char port);

// send stop song
void _midi_tx_stop_song(unsigned char port);

// send active sensing
void _midi_tx_active_sensing(unsigned char port);

// send system reset
void _midi_tx_system_reset(unsigned char port);

// send a debug string
void _midi_tx_debug(unsigned char port,
		char *str);

#endif
