/*
 * K1 Mixer Interface - Utility Functions
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 *
 */
#include "utils.h"
 
int clamp(int num, int low, int high) {
	if(num > high) return high;
	if(num < low) return low;
	return num;
}
