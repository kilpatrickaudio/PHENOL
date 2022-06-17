/*
 * K65 Phenol - Scale Processing
 *
 * Copyright 2015: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#include "scale.h"

#define SCALE_RAMP_VAL_MAX 4095
#define SCALE_NUM_OCTAVES 11
#define SCALE_NUM_SEMIS 12
#define SCALE_OCT_SIZE 408
#define SCALE_SEMI_SIZE 34
int scale_oct[SCALE_NUM_OCTAVES];  // offset vals for each octave
int scale_semi[SCALE_NUM_SEMIS];  // offset vals for each semitone

// init the scale
void scale_init(void) {
	int i;

	// build scale tables
	int count = 0;
	for(i = 0; i < SCALE_NUM_OCTAVES; i ++) {
		scale_oct[i] = count;
		count += SCALE_OCT_SIZE;
	}
	count = 0;
	for(i = 0; i < SCALE_NUM_SEMIS; i ++) {
		scale_semi[i] = count;
		count += SCALE_SEMI_SIZE;
	}
}

// find the scale octave of the current value
int scale_find_octave(int val) {	
	int min, max, oct;
	if(val < 0 || val > SCALE_RAMP_VAL_MAX) {
		return 0;
	}
	min = 0;
	max = SCALE_NUM_OCTAVES;
	while(min <= max) {
		oct = min + ((max - min) >> 1);
		if(val >= scale_oct[oct]) {
			min = oct + 1;
		}
		else if(val < (scale_oct[oct] - SCALE_OCT_SIZE)) {
			max = oct - 1;
		}
		else {
			return oct - 1;
		}
	}
	return oct - 1;
}

// find the scale semitone in the current octave
int scale_find_semitone(int oct, int val) {
	int min, max, semi;
	if(oct < 0 || oct >= SCALE_NUM_OCTAVES) {
		return 0;
	}
	if(val < 0 || val > SCALE_RAMP_VAL_MAX) {
		return 0;
	}
	int baseval = val - scale_oct[oct];
	min = 0;
	max = SCALE_NUM_SEMIS;
	while(min <= max) {
		semi = min + ((max - min) >> 1);
		if(baseval >= scale_semi[semi]) {
			min = semi + 1;
		}
		else if(baseval < (scale_semi[semi] - SCALE_SEMI_SIZE)) {
			max = semi - 1;
		}
		else {
			return semi - 1;
		}
	}
	return semi - 1;
}

// find the note (0-120) of the value (0-4095)
int scale_find_note(int val) {
	int oct, semi;
	oct = scale_find_octave(val);
	semi = scale_find_semitone(oct, val);
	return (oct * 12) + semi;
}