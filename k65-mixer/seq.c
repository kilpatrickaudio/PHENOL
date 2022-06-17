/*
 * PHENOL Mini Sequencer
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 * 
 *	- the clock out can make clocks internally
 *	- record lit means there are contents
 * 	- to erase
 * 		- hold record down - blinks 3 times
 * 		- record is not lit meaning memory is blank
 *	- realtime record
 *		- press record + play - blinks that it is ready
 *		- once first note is heard record + play is solid
 *		- start playing notes to record note/len/timing
 *		- press record to stop (or play?)
 *	- step record mode
 *		- press record - blinks that it is ready
 *		- play notes to record
 *		- press record to stop
 *	- to play
 *		- tap to play once - play LED on solid
 *		- press and hold to loop - play LED blinks
 *	- controls
 *		- speed CC
 *			- speed CC for playback with no clock
 *			- speed CC in clock mode it acts as a clock div
 *	- clock
 *		- internal
 *		- external
 * 
 */
#include "seq.h"
#include "voice.h"
#include "ioctl.h"
#include "midi_clock.h"
#include "phenol_midi.h"
#include <inttypes.h>

//#define SEQ_HOST_DEBUG
//#define SEQ_MIDI_DEBUG

// debugging
#ifdef SEQ_HOST_DEBUG
#include "log.h"
#endif
#ifdef SEQ_MIDI_DEBUG
#include "midi.h"
#include <stdio.h>
#endif

// sequencer states
#define SEQ_STATE_IDLE 0
#define SEQ_STATE_ERASE 1
#define SEQ_STATE_RT_REC_STBY 2
#define SEQ_STATE_RT_REC_RUN 3
#define SEQ_STATE_STEP_REC_STBY 4
#define SEQ_STATE_STEP_REC_RUN 5
#define SEQ_STATE_PLAY_ONCE 6
#define SEQ_STATE_PLAY_LOOP 7
#define SEQ_STATE_PLAY_ONCE_END 8
#define SEQ_STATE_PLAY_LOOP_END 9
#define SEQ_STATE_REC_END 10
int seq_state;
// button handling
#define SEQ_BUTTON_HOLD_TIMEOUT 500
int seq_rec_button_timer;
int seq_play_button_timer;
int seq_rec_button_state;  // record button state - 1 = pressed, 0 = not pressed
int seq_play_button_state;  // play button state - 1 = pressed, 0 = not pressed
int seq_button_lockout;  // do not process more event
// state
int seq_run_timer;  // a timer for record or playback (in ms)
int seq_play_event_pos;  // the event position for playback
int seq_last_event_time;  // run timer value at last event
int seq_play_transpose;  // transpose the playback up or down
#define SEQ_PLAY_TRANSPOSE_BASE 60  // the base note for transposing up or down
// events
#define SEQ_EVENT_NOTE_OFF 0x80
#define SEQ_EVENT_NOTE_ON 0x90
#define SEQ_EVENT_PITCH_BEND 0xe0
#define SEQ_EVENT_REST 0xfe
#define SEQ_EVENT_END 0xff
#define SEQ_NOTE_VEL 64
#define SEQ_STEP_REC_STEP_TIME 6  // 6 clock ticks
#define SEQ_MAX_EVENTS 1024
struct seq_event {
    uint16_t time;  // delta time since last event (in ms)
    uint8_t type;  // event type
    uint8_t value;  // event value
};
struct seq_event seq_event_buf[SEQ_MAX_EVENTS];
int seq_num_events;
int seq_startup;

// local functions
void seq_change_state(int newstate);
void seq_record_event(int event_type, int value, int step_rec);
void seq_play_events(void);

// init the sequencer
void seq_init(void) {
    seq_change_state(SEQ_STATE_IDLE);
    seq_rec_button_timer = 0;
    seq_play_button_timer = 0;
    seq_rec_button_state = 0;
    seq_play_button_state = 0;
    seq_button_lockout = 0;
    seq_run_timer = 0;
    seq_play_event_pos = 0;
    seq_num_events = 0;
    seq_play_transpose = 0;
    seq_startup = 5000;
}

// run the timer task - 1000us
void seq_timer_task(void) {
    if(seq_startup) {
        seq_startup --;
    }

    //
    // handle input
    //
    // handle button lockout
    if(seq_button_lockout) {
        // both buttons have cleared - reset everythign
        if(seq_rec_button_state == 0 && seq_play_button_state == 0) {
            seq_rec_button_timer = 0;
            seq_play_button_timer = 0;
            seq_button_lockout = 0;
        }
    }
    // no lockout
    else {    
        //
        // handle normal presses and releases
        //
        // record pressed
        if(seq_rec_button_state) {
            seq_rec_button_timer ++;
            if(seq_rec_button_timer == SEQ_BUTTON_HOLD_TIMEOUT) {
                // startup mode change
                if(seq_startup) {
                    if(phenol_midi_get_stop_ignore()) {
                        phenol_midi_set_stop_ignore(0);  // disable
                    }
                    else {
                        phenol_midi_set_stop_ignore(1);  // enable
                        ioctl_set_midi_rec_led(SEQ_LED_BLINK_ONCE);
                        ioctl_set_midi_play_led(SEQ_LED_BLINK_ONCE);
                    }
                }
                // normal
                else {
                    // erase mode
                    seq_change_state(SEQ_STATE_ERASE);
                }
                seq_button_lockout = 1;
            }
        }
        // record released
        else {
            if(seq_rec_button_timer) {
                // play is also pressed
                if(seq_play_button_state) {
                    // realtime record mode
                    seq_change_state(SEQ_STATE_RT_REC_STBY);
                    seq_button_lockout = 1;
                }
                // play is not pressed
                else {
                    // cancel recording
                    if(seq_state == SEQ_STATE_STEP_REC_STBY || 
                            seq_state == SEQ_STATE_RT_REC_STBY ||
                            seq_state == SEQ_STATE_STEP_REC_RUN ||
                            seq_state == SEQ_STATE_RT_REC_RUN) {
                        seq_change_state(SEQ_STATE_IDLE);
                    }
                    // step record mode
                    else {
                        seq_change_state(SEQ_STATE_STEP_REC_STBY);
                        seq_button_lockout = 1;
                    }
                }
                seq_rec_button_timer = 0;
            }
        }
        // play pressed
        if(seq_play_button_state) {
            seq_play_button_timer ++;
            if(seq_play_button_timer == SEQ_BUTTON_HOLD_TIMEOUT) {
                // loop mode
                seq_change_state(SEQ_STATE_PLAY_LOOP);
                seq_button_lockout = 1;
            }
        }
        // play released
        else {
            if(seq_play_button_timer) {
                // rec is also pressed
                if(seq_rec_button_state) {
                    // realtime record mode
                    seq_change_state(SEQ_STATE_RT_REC_STBY);
                    seq_button_lockout = 1;
                }
                // stop a one-shot
                else if(seq_state == SEQ_STATE_PLAY_ONCE) {
                    seq_change_state(SEQ_STATE_PLAY_ONCE_END);
                }
                // stop a playing loop
                else if(seq_state == SEQ_STATE_PLAY_LOOP) {
                    seq_change_state(SEQ_STATE_PLAY_LOOP_END);
                }
                // start playing
                else {
                    // play mode
                    seq_change_state(SEQ_STATE_PLAY_ONCE);
                    seq_button_lockout = 1;
                }                
                seq_play_button_timer = 0;
            }
        }
    } 
}

// handle the record button
void seq_rec_button(int pressed) {
    if(pressed) {
        seq_rec_button_state = 1;
    }
    else {
        seq_rec_button_state = 0;
    }
}

// handle the play button
void seq_play_button(int pressed) {
    if(pressed) {
        seq_play_button_state = 1;
    }
    else {
        seq_play_button_state = 0;
    }
}

// handle a MIDI start event
void seq_clock_handle_midi_start(void) {
    // no action     
}

// handle a MIDI stop event
void seq_clock_handle_midi_stop(void) {
    seq_change_state(SEQ_STATE_IDLE);
}

// handle a MIDI continue event
void seq_clock_handle_midi_continue(void) {
    // no action     
}

// handle a MIDI clock fail event
void seq_clock_handle_clock_fail(void) {
    seq_change_state(SEQ_STATE_IDLE);  // stop and reset sequencer
}

// handle a clock tick
void seq_clock_tick(void) {
    switch(seq_state) {
        case SEQ_STATE_RT_REC_RUN:
            seq_run_timer ++;
            break;
        case SEQ_STATE_PLAY_ONCE:
        case SEQ_STATE_PLAY_LOOP:
            seq_play_events();        
            seq_run_timer ++;
            break;
    }
}

//
// local functions
//
// change the sequencer state
void seq_change_state(int newstate) {
#ifdef SEQ_MIDI_DEBUG
    char strtmp[64];
#endif
    if(newstate == seq_state) {
        return;
    }

    // see if we need to stop recording before entering another state
    if(seq_state == SEQ_STATE_RT_REC_RUN ||
            seq_state == SEQ_STATE_STEP_REC_RUN) {
#ifdef SEQ_HOST_DEBUG
        log_debug("ending recording");
#endif
#ifdef SEQ_MIDI_DEBUG
        _midi_tx_debug(MIDI_PORT_USB, "ending recording");
#endif
        // insert end event in event list
        if(seq_state == SEQ_STATE_STEP_REC_RUN) {
            seq_record_event(SEQ_EVENT_END, 0, 1);
        }
        else {
            seq_record_event(SEQ_EVENT_END, 0, 0);
        }
        seq_state = SEQ_STATE_IDLE;  // in case the new state is invalid
    }

    // switch to new state
    switch(newstate) {
        case SEQ_STATE_IDLE:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: idle");
#endif
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: idle");
#endif
            voice_all_notes_off(0);
            ioctl_set_midi_rec_led(SEQ_LED_OFF);
            ioctl_set_midi_play_led(SEQ_LED_OFF);
            voice_set_transpose(0, 0);  // reset transpose but remember current setting
            seq_state = SEQ_STATE_IDLE;
            break;
        case SEQ_STATE_ERASE:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: erase");
#endif            
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: erase");
#endif
            seq_change_state(SEQ_STATE_IDLE);
            seq_num_events = 0;
            ioctl_set_midi_rec_led(SEQ_LED_BLINK_ONCE);
            ioctl_set_midi_play_led(SEQ_LED_OFF);
            seq_change_state(SEQ_STATE_IDLE);
            return;
        case SEQ_STATE_RT_REC_STBY:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: realtime record standby");
#endif
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: realtime record standby");
#endif
            seq_change_state(SEQ_STATE_IDLE);
            ioctl_set_midi_rec_led(SEQ_LED_BLINK);
            ioctl_set_midi_play_led(SEQ_LED_BLINK);
            // reset transpose
            seq_play_transpose = 0;
            voice_set_transpose(0, seq_play_transpose);
            seq_state = SEQ_STATE_RT_REC_STBY;
            break;
        case SEQ_STATE_RT_REC_RUN:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: realtime record run");
#endif            
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: realtime record run");
#endif
            seq_run_timer = 0;
            seq_last_event_time = 0;
            seq_num_events = 0;
            ioctl_set_midi_rec_led(SEQ_LED_ON);
            ioctl_set_midi_play_led(SEQ_LED_ON);
            seq_state = SEQ_STATE_RT_REC_RUN;
            break;
        case SEQ_STATE_STEP_REC_STBY:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: step record standby");
#endif            
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: step record standby");
#endif
            seq_change_state(SEQ_STATE_IDLE);
            ioctl_set_midi_rec_led(SEQ_LED_BLINK);
            ioctl_set_midi_play_led(SEQ_LED_OFF);
            // reset transpose
            seq_play_transpose = 0;
            voice_set_transpose(0, seq_play_transpose);
            seq_state = SEQ_STATE_STEP_REC_STBY;
            break;
        case SEQ_STATE_STEP_REC_RUN:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: step record  run");
#endif
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: step record run");
#endif
            seq_run_timer = 0;
            seq_last_event_time = 0;
            seq_num_events = 0;
            ioctl_set_midi_rec_led(SEQ_LED_ON);
            ioctl_set_midi_play_led(SEQ_LED_OFF);
            seq_state = SEQ_STATE_STEP_REC_RUN;
            break;
        case SEQ_STATE_PLAY_ONCE:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: play once");
#endif
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: play once");
#endif
            seq_change_state(SEQ_STATE_IDLE);
            // only play if we have events
            if(seq_num_events > 0) {
                // reset transpose
                seq_play_transpose = 0;
                voice_set_transpose(0, seq_play_transpose);
                // start playback
                seq_run_timer = 0;
                seq_play_event_pos = 0;
                seq_last_event_time = 0;
                ioctl_set_midi_rec_led(SEQ_LED_OFF);
                ioctl_set_midi_play_led(SEQ_LED_ON);
                seq_state = SEQ_STATE_PLAY_ONCE;
                midi_clock_ungate();  // cause the ext. clock to ungate
            }
            break;
        case SEQ_STATE_PLAY_LOOP:
#ifdef SEQ_HOST_DEBUG
            log_debug("seq state: play loop");
#endif            
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "seq state: play loop");
#endif
            // if we're already playing a one-shot, just enable loop mode without restarting
            if(seq_state == SEQ_STATE_PLAY_ONCE) {
                seq_state = SEQ_STATE_PLAY_LOOP;
                ioctl_set_midi_rec_led(SEQ_LED_OFF);
                ioctl_set_midi_play_led(SEQ_LED_BLINK);
            }
            else {
                seq_change_state(SEQ_STATE_IDLE);
                // only play if we have events
                if(seq_num_events > 0) {
                    // reset transpose
                    seq_play_transpose = 0;
                    voice_set_transpose(0, seq_play_transpose);
                    // start playback
                    seq_run_timer = 0;
                    seq_play_event_pos = 0;
                    seq_last_event_time = 0;
                    seq_state = SEQ_STATE_PLAY_LOOP;
                    ioctl_set_midi_rec_led(SEQ_LED_OFF);
                    ioctl_set_midi_play_led(SEQ_LED_BLINK);
                    midi_clock_ungate();  // cause the ext. clock to ungate
                }
            }
            break;
        case SEQ_STATE_PLAY_ONCE_END:
            ioctl_set_midi_rec_led(SEQ_LED_OFF);
            ioctl_set_midi_play_led(SEQ_LED_OFF);
            seq_change_state(SEQ_STATE_IDLE);
            break;
        case SEQ_STATE_PLAY_LOOP_END:
            seq_state = SEQ_STATE_PLAY_ONCE;  // cause playback to stop at end
            break;
        default:
#ifdef SEQ_HOST_DEBUG
            log_debug("unknown state: %d", newstate);
#endif
#ifdef SEQ_MIDI_DEBUG
            sprintf(strtmp, "unknown state: %d", newstate);
            _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif
            break;
    }
}

// record an event at the current time
void seq_record_event(int event_type, int value, int step_rec) {
#ifdef SEQ_MIDI_DEBUG
    char strtmp[64];
#endif
    // we have space to record more events
    if(seq_num_events < (SEQ_MAX_EVENTS - 2)) {
        seq_event_buf[seq_num_events].type = event_type;
        seq_event_buf[seq_num_events].value = value;
        // step record
        if(step_rec) {
            // force first event at 0 time delta
            if(seq_num_events == 0) {
                seq_event_buf[seq_num_events].time = 0;
            }
            // other events go forward 
            else {
                seq_event_buf[seq_num_events].time = SEQ_STEP_REC_STEP_TIME >> 1;
            }
            // note on events are automatically stopped 1/2 a step later
            if(event_type == SEQ_EVENT_NOTE_ON) {
                // insert note off
                seq_num_events ++;
                seq_event_buf[seq_num_events].type = SEQ_EVENT_NOTE_OFF;
                seq_event_buf[seq_num_events].value = value;
                seq_event_buf[seq_num_events].time = SEQ_STEP_REC_STEP_TIME >> 1;
            }
            // rest event
            else if(event_type == SEQ_EVENT_REST) {
                // make the delta the full step length instead of half a step
                seq_event_buf[seq_num_events].time = SEQ_STEP_REC_STEP_TIME;
            }
            // end the sequence
            else if(event_type == SEQ_EVENT_END) {
                // we can't have a sequence with no notes
                if(seq_num_events == 0) {
                    return;
                }
                seq_event_buf[seq_num_events].type = SEQ_EVENT_END;
                // back up 1 tick so next loop starts on the note start time
                seq_event_buf[seq_num_events].time = (SEQ_STEP_REC_STEP_TIME >> 1) - 1;
            }
            // other events are ignored
            else {
                return;
            }
        }
        // realtime record
        else {
            seq_event_buf[seq_num_events].time = seq_run_timer - seq_last_event_time;            
        }
#ifdef SEQ_HOST_DEBUG
        log_debug("recording event - num: %d - type: 0x%02x - value: 0x%02x - time: %d",
            seq_num_events,
            seq_event_buf[seq_num_events].type,
            seq_event_buf[seq_num_events].value,
            seq_event_buf[seq_num_events].time);            
#endif
#ifdef SEQ_MIDI_DEBUG
        sprintf(strtmp, "recording event - num: %d - type: 0x%02x - value: 0x%02x - time: %d",
            seq_num_events,
            seq_event_buf[seq_num_events].type,
            seq_event_buf[seq_num_events].value,
            seq_event_buf[seq_num_events].time);            
        _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif
        seq_last_event_time = seq_run_timer;
        seq_num_events ++;
        if(seq_num_events == (SEQ_MAX_EVENTS - 1)) {
            // insert end marker
            seq_event_buf[seq_num_events].type = SEQ_EVENT_END;
            seq_event_buf[seq_num_events].value = 0;
            if(step_rec) {
                seq_event_buf[seq_num_events].time = SEQ_STEP_REC_STEP_TIME;
            }
            else {
                seq_event_buf[seq_num_events].time = 0;
            }
#ifdef SEQ_HOST_DEBUG
            log_debug("event list full - stopping record");
#endif
#ifdef SEQ_MIDI_DEBUG
            _midi_tx_debug(MIDI_PORT_USB, "event list full - stopping record");
#endif
            seq_change_state(SEQ_STATE_IDLE);
        }
    }
    else {
        seq_change_state(SEQ_STATE_IDLE);
    }
}

// play events if possible
void seq_play_events(void) {
#ifdef SEQ_MIDI_DEBUG
    char strtmp[64];
#endif
    int temp;
    while(1) {
        // there are more events to play
        if(seq_play_event_pos < seq_num_events) {
            // if we are on internal and it's event 0, pulse the clock output
            if(seq_play_event_pos == 0 && midi_clock_get_internal()) {
                midi_clock_pulse_clock_out();
            }

            // execute the event
            if(seq_event_buf[seq_play_event_pos].time <= (seq_run_timer - seq_last_event_time)) {
                switch(seq_event_buf[seq_play_event_pos].type) {
                    case SEQ_EVENT_NOTE_OFF:
#ifdef SEQ_HOST_DEBUG
                        log_debug("play note off - event: %d - time: %d - value: 0x%02x", 
                            seq_play_event_pos,
                            seq_run_timer,
                            seq_event_buf[seq_play_event_pos].value);
#endif
#ifdef SEQ_MIDI_DEBUG
                        sprintf(strtmp, "play note off - event: %d - time: %d - value: 0x%02x", 
                            seq_play_event_pos,
                            seq_run_timer,
                            seq_event_buf[seq_play_event_pos].value);
                        _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif         
                        voice_note_off(0, seq_event_buf[seq_play_event_pos].value);
                        break;
                    case SEQ_EVENT_NOTE_ON:
#ifdef SEQ_HOST_DEBUG
                        log_debug("play note on - event: %d - time: %d - value: 0x%02x", 
                            seq_play_event_pos,
                            seq_run_timer,
                            seq_event_buf[seq_play_event_pos].value);
#endif
#ifdef SEQ_MIDI_DEBUG
                        sprintf(strtmp, "play note on - event: %d - time: %d - value: 0x%02x", 
                            seq_play_event_pos,
                            seq_run_timer,
                            seq_event_buf[seq_play_event_pos].value);
                        _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif
                        // update transpose offset in voice module
                        voice_set_transpose(0, seq_play_transpose);
                        // turn on note
                        voice_note_on(0, seq_event_buf[seq_play_event_pos].value, SEQ_NOTE_VEL);
                        break;
                    case SEQ_EVENT_PITCH_BEND:
                        temp = seq_event_buf[seq_play_event_pos].value << 8;
#ifdef SEQ_HOST_DEBUG
                        log_debug("play pitch bend - event: %d - time: %d - value: %d", 
                            seq_play_event_pos,
                            seq_run_timer,
                            temp);
#endif
#ifdef SEQ_MIDI_DEBUG
                        sprintf(strtmp, "play pitch bend - event: %d - time: %d - value: %d", 
                            seq_play_event_pos,
                            seq_run_timer,
                            temp);                  
                        _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif
                        voice_pitch_bend(0, temp);
                        break;
                    case SEQ_EVENT_REST:
                        // do nothing
#ifdef SEQ_HOST_DEBUG
                        log_debug("play rest");
#endif
#ifdef SEQ_MIDI_DEBUG
                        _midi_tx_debug(MIDI_PORT_USB, "play rest");
#endif
                        break;
                    case SEQ_EVENT_END:
                        // let it go because the routine below will catch us
                        break;
                }
                seq_play_event_pos ++;
                seq_last_event_time = seq_run_timer;
            }
            // event is in the future
            else {
                return;
            }
        }
        // all done playing
        else {
#ifdef SEQ_HOST_DEBUG
            log_debug("playback end - time: %d", seq_run_timer);
#endif
#ifdef SEQ_MIDI_DEBUG
            sprintf(strtmp, "playback end - time: %d", seq_run_timer);
            _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif
            voice_all_notes_off(0);
            // loop
            if(seq_state == SEQ_STATE_PLAY_LOOP) {
                seq_play_event_pos = 0;
            }
            // stop
            else {
                seq_change_state(SEQ_STATE_IDLE);
            }
            return;
        }
    }
}            

//
// MIDI handlers - sequencer-specific messages
// pass through this module always
//
// note off
void seq_midi_note_off(int note) {
#ifdef SEQ_MIDI_DEBUG
    char strtmp[64];
#endif
#ifdef SEQ_HOST_DEBUG
    log_debug("seq note off: %d", note);
#endif    
#ifdef SEQ_MIDI_DEBUG
    sprintf(strtmp, "seq_note_off: %d", note);
    _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif

    //
    // handle note
    //
    // realtime record
    if(seq_state == SEQ_STATE_RT_REC_RUN) {
        seq_record_event(SEQ_EVENT_NOTE_OFF, note, 0);
        voice_note_off(0, note);
    }
    // step record
    else if(seq_state == SEQ_STATE_STEP_REC_RUN) {
         // note off inserted automatically during playback
        voice_note_off(0, note);
    }
    // playback - don't echo anything
    else if(seq_state == SEQ_STATE_PLAY_ONCE || 
            seq_state == SEQ_STATE_PLAY_LOOP) {
        // do nothing
    }
    // live playing
    else {
        voice_note_off(0, note);
    }
}

// note on
void seq_midi_note_on(int note) {
#ifdef SEQ_MIDI_DEBUG
    char strtmp[64];
#endif
#ifdef SEQ_HOST_DEBUG
    log_debug("seq note on: %d", note);
#endif
#ifdef SEQ_MIDI_DEBUG
    sprintf(strtmp, "seq note on: %d", note);
    _midi_tx_debug(MIDI_PORT_USB, strtmp);
#endif

    // activate recording
    if(seq_state == SEQ_STATE_RT_REC_STBY) {
        seq_change_state(SEQ_STATE_RT_REC_RUN);
    }
    else if(seq_state == SEQ_STATE_STEP_REC_STBY) {
        seq_change_state(SEQ_STATE_STEP_REC_RUN);
    }
    
    //
    // handle note
    //
    // realtime record
    if(seq_state == SEQ_STATE_RT_REC_RUN) {
        seq_record_event(SEQ_EVENT_NOTE_ON, note, 0);
        voice_note_on(0, note, SEQ_NOTE_VEL);
    }
    // step record
    else if(seq_state == SEQ_STATE_STEP_REC_RUN) {
        seq_record_event(SEQ_EVENT_NOTE_ON, note, 1);
        voice_note_on(0, note, SEQ_NOTE_VEL);
    }
    // playback
    else if(seq_state == SEQ_STATE_PLAY_ONCE || 
            seq_state == SEQ_STATE_PLAY_LOOP) {
        // transpose playback
        seq_play_transpose = (note - SEQ_PLAY_TRANSPOSE_BASE);  // transpose during play
    }
    // live playing
    else {
        voice_note_on(0, note, SEQ_NOTE_VEL);
    }
}

// pitch bend - bend = 14 bit signed value
void seq_midi_pitch_bend(int bend) {
    int temp = bend >> 8;  // reduce to 8 bit
    // handle MIDI
    if(seq_state == SEQ_STATE_RT_REC_RUN) {
        seq_record_event(SEQ_EVENT_PITCH_BEND, temp, 0);
        voice_pitch_bend(0, temp << 8);
    }
    else if(seq_state == SEQ_STATE_STEP_REC_RUN) {
        // not supported in step record mode
    }
    else {
        voice_pitch_bend(0, temp << 8);
    }
}

// sustain pedal
void seq_midi_sustain_pedal(int value) {
    // realtime record
    if(seq_state == SEQ_STATE_RT_REC_RUN) {
        // no action
    }
    // step record
    else if(seq_state == SEQ_STATE_STEP_REC_RUN) {
        if(value == 127) {
            // insert a rest in the recording
            seq_record_event(SEQ_EVENT_REST, 0, 1);
        }
    }
    // playback loop enable
    else if(seq_state == SEQ_STATE_PLAY_ONCE) {
        if(value == 127) {
            seq_change_state(SEQ_STATE_PLAY_LOOP);
        }
    }
    // playback loop end
    else if(seq_state == SEQ_STATE_PLAY_LOOP) {
        if(value == 127) {
            seq_change_state(SEQ_STATE_PLAY_LOOP_END);
        }
    }
    // live playing
    else {
		voice_damper(0, value);
    }
}