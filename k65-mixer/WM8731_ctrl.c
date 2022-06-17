/*
 * WM8731 Control Driver
 *
 * Copyright 2013: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#include "WM8731_ctrl.h"
#include "audio_sys_ctrl.h"
#include "audio_sys.h"
#include "HardwareProfile.h"

// local functions
void WM8731_ctrl_write_cmd(unsigned char reg, int val);

// initialize the WM8731 control driver
void WM8731_ctrl_init(void) {
}

// start the WM8731 codec - set for normal operation
void WM8731_ctrl_start(void) {
	// reset codec
	WM8731_ctrl_write_cmd(WM_RESET, 0);

	// output power up - keep oscillator and clkout off
	WM8731_ctrl_write_cmd(WM_POWER_DOWN_CTRL,
		WM_LINEIN_ON | WM_MIC_OFF | WM_ADC_ON | WM_DAC_ON | \
		WM_OUTPUTS_ON | WM_OSC_OFF | WM_CLKOUT_OFF | WM_POWER_OFF);
	
	// deactivate interface
	WM8731_ctrl_write_cmd(WM_ACTIVE_CTRL, WM_ACTIVE_OFF);

	// set analog path
	WM8731_ctrl_write_cmd(WM_ANALOG_PATH_CTRL, 
		WM_MIC_BOOST_OFF | WM_MIC_MUTE_ON | WM_INPUT_LINE |\
		WM_BYPASS_OFF | WM_DACSEL_ON | WM_SIDETONE_OFF | WM_SIDETONE_ATT_6DB);

	// set digital path
	WM8731_ctrl_write_cmd(WM_DIGITAL_PATH_CTRL,
		WM_ADC_HPF_OFF | WM_DEEMP_OFF | WM_HPOR_OFF);  // with HPF on there is a tone audible on ADC!

	// master and slave mode
#ifdef WM8731_I2S_MASTER
#warning WM8731 running in I2S master mode
	WM8731_ctrl_write_cmd(WM_DIGITAL_INTERFACE_FORMAT,
		WM_FORMAT_I2S | WM_BITLEN_24 | WM_LR_PHASE_NORM_I2S | \
		WM_LRSWAP_NORM | WM_MASTER_MODE | WM_BCLK_NORM);
#elif defined(WM8731_I2S_SLAVE)
#warning WM8731 running in I2S slave mode
	WM8731_ctrl_write_cmd(WM_DIGITAL_INTERFACE_FORMAT,
		WM_FORMAT_I2S | WM_BITLEN_16 | WM_LR_PHASE_NORM_I2S | \
		WM_LRSWAP_NORM | WM_SLAVE_MODE | WM_BCLK_NORM);
#else
#error either WM8731_I2S_MASTER or WM8731_I2S_SLAVE must be defined
#endif

	// default sample rate
#ifdef WM8731_DEFAULT_SR_32
#warning WM8731 defaulting to sample rate 32kHz
	WM8731_ctrl_set_samplerate(WM8731_SR_32);
#elif defined(WM8731_DEFAULT_SR_44_1)
#warning WM8731 defaulting to sample rate 44.1kHz
	WM8731_ctrl_set_samplerate(WM8731_SR_44_1);
#elif defined(WM8731_DEFAULT_SR_48)
#warning WM8731 defaulting to sample rate 48kHz
	WM8731_ctrl_set_samplerate(WM8731_SR_48);
#elif defined(WM8731_DEFAULT_SR_88_2)
#warning WM8731 defaulting to sample rate 88.2kHz
	WM8731_ctrl_set_samplerate(WM8731_SR_88_2);
#elif defined(WM8731_DEFAULT_SR_96)
#warning WM8731 defaulting to sample rate 96kHz
	WM8731_ctrl_set_samplerate(WM8731_SR_96);
#else
#error one of WM8731_DEFAULT_SR_xxxx (32, 44_1, 48, 88_2, 96) must be defined
#endif

	// set line in L and R volume
	WM8731_ctrl_write_cmd(WM_LEFT_LINE_IN,
		WM_LINE_IN_0DB | WM_LINE_IN_0DB | WM_LINE_LR_BOTH);

	// set headphone volume
	WM8731_ctrl_write_cmd(WM_LEFT_HP_OUT, 
		WM_HP_0DB | WM_HP_LZC_OFF | WM_HP_LR_BOTH);

	// activate interface
	WM8731_ctrl_write_cmd(WM_ACTIVE_CTRL, WM_ACTIVE_ON);

	// output power up - keep oscillator and clkout off
	WM8731_ctrl_write_cmd(WM_POWER_DOWN_CTRL,
		WM_LINEIN_ON | WM_MIC_OFF | WM_ADC_ON | WM_DAC_ON | \
		WM_OUTPUTS_ON | WM_OSC_OFF | WM_CLKOUT_OFF | WM_POWER_ON); 
}

// set samplerate
void WM8731_ctrl_set_samplerate(int rate) {
	int mode;
	int base;
	int samp;
#ifdef WM8731_MCLK_11_2896
#warning WM8731 MCLK set to 11.2896MHz
	mode = WM_NORMAL_MODE;
	switch(rate) {
		case WM8731_SR_32:
			// not supported
			return;
		case WM8731_SR_44_1:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 8;
			break;
		case WM8731_SR_48:
			// not supported
			return;
		case WM8731_SR_88_2:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 8;
			break;
		case WM8731_SR_96:
			// not supported
			return;
		default:
			return;
	}
#elif defined(WM8731_MCLK_12_0000)
#warning WM8731 MCLK set to 12.000MHz
	mode = WM_USB_MODE;
	switch(rate) {
		case WM8731_SR_32:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 6;
			break;
		case WM8731_SR_44_1:
			base = WM_OVERSAMPLE_RATE_384;
			samp = 8;
			break;
		case WM8731_SR_48:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 0;
			break;
		case WM8731_SR_88_2:
			base = WM_OVERSAMPLE_RATE_384;
			samp = 15;
			break;
		case WM8731_SR_96:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 7;
			break;
		default:
			return;
	}
#elif defined(WM8731_MCLK_12_2880)
#warning WM8731 MCLK set to 12.2880MHz
	mode = WM_NORMAL_MODE;
	switch(rate) {
		case WM8731_SR_32:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 6;
			break;
		case WM8731_SR_44_1:
			// not supported
			return;
		case WM8731_SR_48:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 0;
			break;
		case WM8731_SR_88_2:
			// not supported
			return;
		case WM8731_SR_96:
			base = WM_OVERSAMPLE_RATE_256;
			samp = 7;
			break;
		default:
			return;
	}
#else
#error one of WM8731_MCLK_xxxx (11_2896, 12_0000, 12_2880) must be defined
#endif
	// set sampling control
	WM8731_ctrl_write_cmd(WM_SAMPLING_CTRL,
		mode | base | (samp << 2) | WM_CLKIDIV2_OFF | WM_CLKODIV2_OFF);
}

// write a command to the codec over i2c
void WM8731_ctrl_write_cmd(unsigned char reg, int val) {
	unsigned char data[2];
	data[0] = (reg << 1) | (val >> 8);
	data[1] = val & 0xff;
	audio_sys_write_ctrl(WM8731_ADDR_SELECT, data, 2);
}
