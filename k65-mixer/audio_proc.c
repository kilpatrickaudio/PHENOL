/*
 * K65 Phenol Mixer - Audio Processing
 *
 * Copyright 2014: Kilpatrick 
 * Written by: Andrew Kilpatrick
 *
 */
#include "audio_proc.h"
#include "audio_sys.h"
#include "ioctl.h"
#include "g711.h"
#include <dsplib_def.h>
#include <inttypes.h>
#include <plib.h>

// uncomment one of these
// - default is to use 8 bit delay memory
#define DELAY_MEM_ULAW  // preferred - thump on startup
//#define DELAY_MEM_ALAW  // alternate - no thump on startup
//#define DELAY_MEM_NONE  // testing with no delay
#define DELAY_FILT_K 0.70

// new pot smoothing
#define MIX_SMOOTH
#define MIX_FILTER_SHIFT 6

// misc
#define DELAY_LED_BLINK_TIME 64
#define METER_LED_BLINK_TIME 64
#define METER_THRESHOLD 0x0fff

// levels
#define INPUT_BOOST 0x5000
#define OUTPUT_BOOST 0x5fff

// delay effect buffer
#define DELAY_BUF_LEN 16384
#define DELAY_BUF_MASK (DELAY_BUF_LEN - 1)
char delay_buf[DELAY_BUF_LEN];
int delay_buf_p;
int delay_tempo_count;

// audio streaming I/O buffers
extern int16_t audio_rec_buf[AUDIO_BUF_SIZE];
extern int16_t audio_play_buf[AUDIO_BUF_SIZE];
extern int audio_stream_p;
int proc_buf;

// local functions
static inline int32_t scale(int32_t samp, int16_t scale);

// init the audio processor
void audio_proc_init(void) {
	proc_buf = 0;
	delay_tempo_count = 0;
	delay_buf_p = 0;
}

// process a buffer of samples
void audio_proc_process(void) {
	int i;
	int page = (audio_stream_p & (AUDIO_BUF_SIZE >> 1));
	int32_t master_level, in1_level, in2_level;
	int32_t pan1_level, pan2_level, pan1_level_inv, pan2_level_inv;
    int32_t delay_mix, delay_fb;
	int32_t in1, in2, temp, delay_in, delay_out;
	int32_t outL_meter, outR_meter;
	static int32_t delay_time = 1;
	static int32_t delay_filt_hist;
    static int32_t in1_hist = 0;
    static int32_t in2_hist = 0;
    static int32_t pan1l_hist = 0;
    static int32_t pan2l_hist = 0;
    static int32_t pan1r_hist = 0;
    static int32_t pan2r_hist = 0;
    static int32_t master_hist = 0;

	// detect page flips - we are already on this page
	if(proc_buf == page) {
		return;
	}

	// reset level meter stuff
	outL_meter = 0;
	outR_meter = 0;

    //
	// get controls
    //
    // level
	master_level = ioctl_get_pot(POT_MIXER_MASTER) << 6;
	in1_level = ioctl_get_pot(POT_MIXER_IN1_LEVEL) << 7;
	in1_level = mul16(in1_level, in1_level);
	in2_level = ioctl_get_pot(POT_MIXER_IN2_LEVEL) << 7;
	in2_level = mul16(in2_level, in2_level);
    // pan
	pan1_level = ioctl_get_pot(POT_MIXER_PAN1);
	pan2_level = ioctl_get_pot(POT_MIXER_PAN2);
    pan1_level_inv = (0xff - pan1_level) - 0x07;  // fix error in pan
    pan2_level_inv = (0xff - pan2_level) - 0x07;  // fix error in pan
    if(pan1_level_inv < 0) pan1_level_inv = 0;
    if(pan2_level_inv < 0) pan2_level_inv = 0;
    pan1_level = pan1_level << 6;
    pan2_level = pan2_level << 6;
    pan1_level_inv = pan1_level_inv << 6;
    pan2_level_inv = pan2_level_inv << 6;
    // delay
	temp = ioctl_get_pot(POT_MIXER_DELAY_TIME) << 6;
	delay_time = (delay_time - (delay_time >> 6)) + (temp >> 6);
	delay_mix = ioctl_get_pot(POT_MIXER_DELAY_MIX) << 7;  // 0x0000 to 0x7fff
	delay_fb = scale(delay_mix, delay_mix);
	delay_fb = delay_fb >> 2;
	if(delay_mix > 0x3fff) {
		delay_mix = 0x3fff;  // clamp to 50%
	}

	// process a buffer of samples
	for(i = proc_buf; i < (proc_buf + (AUDIO_BUF_SIZE >> 1)); i += 2) {

		in1 = audio_rec_buf[i];
		in2 = audio_rec_buf[i+1];

		// input levels
#ifdef MIX_SMOOTH
#warning MIX_SMOOTH enabled - doing smoothing of pots
        // (out) = (z1) = (((in) - (z1)) * (a0)) + (z1);  // LFP example
        in1_hist = ((in1_level - in1_hist) >> MIX_FILTER_SHIFT) + in1_hist;
		in1 = scale(in1, in1_hist);
        in2_hist = ((in2_level - in2_hist) >> MIX_FILTER_SHIFT) + in2_hist;
		in2 = scale(in2, in2_hist);
#else
		in1 = scale(in1, in1_level);
		in2 = scale(in2, in2_level);
#endif

		// effect output
#ifdef DELAY_MEM_ULAW
		delay_out = g711_mulaw2lin(delay_buf[(delay_buf_p + delay_time) & DELAY_BUF_MASK]);
#elif defined(DELAY_MEM_ALAW)
		delay_out = g711_alaw2lin(delay_buf[(delay_buf_p + delay_time) & DELAY_BUF_MASK]);
#elif defined(DELAY_MEM_NONE)
		delay_out = 0;
#else
		delay_out = delay_buf[(delay_buf_p + delay_time) & DELAY_BUF_MASK] << 8;
#endif

		// delay loop filter
		delay_out = (delay_filt_hist - (delay_filt_hist >> 1)) + (delay_out >> 1);
		delay_filt_hist = delay_out;

		// effect input
		delay_in = scale(in1 + in2, 0x1fff);
		delay_in = scale(delay_in, delay_mix);
		delay_in += scale(delay_out, delay_fb);

#ifdef DELAY_MEM_ULAW
		delay_buf[delay_buf_p] = g711_lin2mulaw(SAT16(delay_in));  // stick audio in the front
#elif defined(DELAY_MEM_ALAW)
		delay_buf[delay_buf_p] = g711_lin2alaw(SAT16(delay_in));  // stick audio in the front
#elif defined(DELAY_MEM_NONE)
		delay_buf[delay_buf_p] = 0;
#else
		delay_buf[delay_buf_p] = SAT16(delay_in) >> 8;  // stick audio in the front
#endif

		// boost inputs for mixdown
		in1 = scale(in1, INPUT_BOOST);  // boost
		in2 = scale(in2, INPUT_BOOST);  // boost

#ifdef MIX_SMOOTH
        pan1l_hist = ((pan1_level_inv - pan1l_hist) >> MIX_FILTER_SHIFT) + pan1l_hist;
        pan2l_hist = ((pan2_level_inv - pan2l_hist) >> MIX_FILTER_SHIFT) + pan2l_hist;
        master_hist = ((master_level - master_hist) >> MIX_FILTER_SHIFT) + master_hist;

		// left output
		temp = scale(in1, pan1l_hist);
		temp += scale(in2, pan2l_hist);
		temp += delay_out;
		temp = scale(temp, master_hist);
#else
		// left output
		temp = scale(in1, pan1_level_inv);
		temp += scale(in2, pan2_level_inv);
		temp += delay_out;
		temp = scale(temp, master_level);
#endif
		temp = scale(temp, OUTPUT_BOOST);  // boost
		temp = SAT16(temp);  // limit to 0dB
		audio_play_buf[i] = temp;

		// level meter
		if(temp > outL_meter) {
			outL_meter = temp;
		}

#ifdef MIX_SMOOTH
        pan1r_hist = ((pan1_level - pan1r_hist) >> MIX_FILTER_SHIFT) + pan1r_hist;
        pan2r_hist = ((pan2_level - pan2r_hist) >> MIX_FILTER_SHIFT) + pan2r_hist;

		// right output
		temp = scale(in1, pan1r_hist);
		temp += scale(in2, pan2r_hist);
		temp += delay_out;
		temp = scale(temp, master_level);
#else
		// right output
		temp = scale(in1, pan1_level);
		temp += scale(in2, pan2_level);
		temp += delay_out;
		temp = scale(temp, master_level);
#endif
		temp = scale(temp, OUTPUT_BOOST);  // boost
		temp = SAT16(temp);  // limit to 0dB
		audio_play_buf[i+1] = temp;

		// level meter
		if(temp > outR_meter) {
			outR_meter = temp;
		}

		// decrement delay pointer
		delay_buf_p = (delay_buf_p - 1) & DELAY_BUF_MASK;

	}

	// blink the delay tempo LED
	delay_tempo_count += AUDIO_BUF_SIZE >> 2;
	if(delay_tempo_count > delay_time) {
		ioctl_set_mixer_delay_led(DELAY_LED_BLINK_TIME);
        // 1.24 rollover fix
		delay_tempo_count = (delay_tempo_count - delay_time) & 0xffff;
	}

	proc_buf = page;

	// level meters - do threshold
	if(outL_meter > METER_THRESHOLD) {
		outL_meter = METER_LED_BLINK_TIME;
	}
	else {
		outL_meter = 0;
	}
	if(outR_meter > METER_THRESHOLD) {
		outR_meter = METER_LED_BLINK_TIME;
	}
	else {
		outR_meter = 0;
	}
	ioctl_set_mixer_output_leds(outL_meter, outR_meter);
}

// scale a sample - samp: 16 bit - scale: 0x0000 to 0x7fff = 0-200%
static inline int32_t scale(int32_t samp, int16_t scale) {
	return (samp * scale) >> 14;
}

// generate silent output
void audio_proc_silence(void) {
	int i;
	int page = (audio_stream_p & (AUDIO_BUF_SIZE >> 1));

	// detect page flips - we are already on this page
	if(proc_buf == page) {
		return;
	}

	// process a buffer of samples
	for(i = proc_buf; i < (proc_buf + (AUDIO_BUF_SIZE >> 1)); i += 2) {
#ifdef DELAY_MEM_ULAW
		delay_buf[delay_buf_p] = g711_lin2mulaw(0);  // silence
#elif defined(DELAY_MEM_ALAW)
		delay_buf[delay_buf_p] = g711_lin2alaw(0);  // silence
#elif defined(DELAY_MEM_NONE)
		delay_buf[delay_buf_p] = 0;  // silence
#else
		delay_buf[delay_buf_p] = SAT16(0) >> 8;  // silence
#endif
		audio_play_buf[i] = 0;  // silence
		audio_play_buf[i+1] = 0;  // silence

		// decrement delay pointer
		delay_buf_p = (delay_buf_p - 1) & DELAY_BUF_MASK;
	}
	proc_buf = page;
}