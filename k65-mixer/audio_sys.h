/*
 * KSEND Audio Subsystem
 *
 * Copyright 2013: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef AUDIO_SYS_H
#define AUDIO_SYS_H

#define AUDIO_BUF_SIZE 128
#define AUDIO_BUF_MASK (AUDIO_BUF_SIZE - 1)

typedef int int32;
typedef long long int64;

// init the audio system - set up I2C
void audio_sys_init(void);

// start the audio system - start I2S
void audio_sys_start(void);

// clamp audio by clipping
static inline int32 audio_clamp(int32 val) {
	if(val > 0x7fffff) return 0x7fffff;
	if(val < -0x800000) return -0x800000;
	return val;
}

// convert 24 bit i2s to int
static inline int32 audio_i2s_to_int(int32 val) {
  if(val & 0x800000) {
    return (val & 0x7fffff) | 0xff800000;
  }
  return val & 0x7fffff;
}

// convert signed into to 24 bit i2s
static inline int32 audio_int_to_i2s(int32 val) {
  if(val < 0) {
    return (val & 0x7fffff) | 0x800000;
  }
  return val & 0x7fffff;
}

#endif
