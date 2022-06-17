/*
 * WM8731 Control Driver
 *
 * Copyright 2013: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * 
 */
#ifndef WM8731_CTRL_H
#define WM8731_CTRL_H

// I2C addresses
// CSB = 0
#define WM8731_ADDR_SELECT		0x1a
// CSB = 1
//#define WM_ADDR_SELECT		0x1b

// set master or slave mode for the codec
//#define WM8731_I2S_MASTER
#define WM8731_I2S_SLAVE

// set the MCLK rate used - for codec divider calculations
//
//  AUDIO_MCLK_0 = 11.2896MHz (44.1/88.2 standard crystal)
//  AUDIO_MCLK_1 = 12.0000MHz (USB clock)
//  AUDIO_MCLK_2 = 12.2880MHz (48/96 standard crystal)
//#define WM8731_MCLK_11_2896
//#define WM8731_MCLK_12_0000
#define WM8731_MCLK_12_2880

// default sampling rate
//#define WM8731_DEFAULT_SR_32
//#define WM8731_DEFAULT_SR_44_1
#define WM8731_DEFAULT_SR_48
//#define WM8731_DEFAULT_SR_88_2
//#define WM8731_DEFAULT_SR_96

//
// register defines
//
#define WM_LEFT_LINE_IN 0x00
#define WM_RIGHT_LINE_IN 0x01
#define WM_LEFT_HP_OUT 0x02
#define WM_RIGHT_HP_OUT 0x03
#define WM_ANALOG_PATH_CTRL 0x04
#define WM_DIGITAL_PATH_CTRL 0x05
#define WM_POWER_DOWN_CTRL 0x06
#define WM_DIGITAL_INTERFACE_FORMAT 0x07
#define WM_SAMPLING_CTRL 0x08
#define WM_ACTIVE_CTRL 0x09
#define WM_RESET 0x0f
//
// register control bits
//
// line in control bits
#define WM_LINE_IN_6DB 0x1b
#define WM_LINE_IN_0DB 0x17
#define WM_LINE_IN_MUTE 0x80
#define WM_LINE_IN_UNMUTE 0x00
#define WM_LINE_LR_BOTH 0x100
#define WM_LINE_LR_SEP 0x000
#define WM_LINE_RL_BOTH 0x100
#define WM_LINE_RL_SEP 0x000
// headphone control bits
#define WM_HP_6DB 0x7f
#define WM_HP_0DB 0x79
#define WM_HP_LZC_ON 0x80
#define WM_HP_LZC_OFF 0x00
#define WM_HP_RZC_ON 0x80
#define WM_HP_RZC_OFF 0x00
#define WM_HP_LR_BOTH 0x100
#define WM_HP_LR_SEP 0x000
#define WM_HP_RL_BOTH 0x100
#define WM_HP_RL_SEP 0x000
// analog audio path control
#define WM_MIC_BOOST_ON 0x01
#define WM_MIC_BOOST_OFF 0x00
#define WM_MIC_MUTE_ON 0x02
#define WM_MIC_MUTE_OFF 0x00
#define WM_INPUT_MIC 0x04
#define WM_INPUT_LINE 0x00
#define WM_BYPASS_ON 0x08
#define WM_BYPASS_OFF 0x00
#define WM_DACSEL_ON 0x10
#define WM_DACSEL_OFF 0x00
#define WM_SIDETONE_ON 0x20
#define WM_SIDETONE_OFF 0x00
#define WM_SIDETONE_ATT_15DB 0xc0
#define WM_SIDETONE_ATT_12DB 0x80
#define WM_SIDETONE_ATT_9DB 0x40
#define WM_SIDETONE_ATT_6DB 0x00
// digital audio path control
#define WM_ADC_HPF_ON 0x00
#define WM_ADC_HPF_OFF 0x01
#define WM_DEEMP_48K 0x06
#define WM_DEEMP_44K 0x04
#define WM_DEEMP_32K 0x02
#define WM_DEEMP_OFF 0x00
#define WM_HPOR_ON 0x10
#define WM_HPOR_OFF 0x00
// power control bits
#define WM_LINEIN_ON 0x00
#define WM_LINEIN_OFF 0x01
#define WM_MIC_ON 0x00
#define WM_MIC_OFF 0x02
#define WM_ADC_ON 0x00
#define WM_ADC_OFF 0x04
#define WM_DAC_ON 0x00
#define WM_DAC_OFF 0x08
#define WM_OUTPUTS_ON 0x00
#define WM_OUTPUTS_OFF 0x10
#define WM_OSC_ON 0x00
#define WM_OSC_OFF 0x20
#define WM_CLKOUT_ON 0x00
#define WM_CLKOUT_OFF 0x40
#define WM_POWER_ON 0x00
#define WM_POWER_OFF 0x80
// digital audio interface format
#define WM_FORMAT_DSP 0x03
#define WM_FORMAT_I2S 0x02
#define WM_FORMAT_MSB_LJ 0x01
#define WM_FORMAT_MSB_RJ 0x00
#define WM_BITLEN_32 0x0c
#define WM_BITLEN_24 0x08
#define WM_BITLEN_20 0x04
#define WM_BITLEN_16 0x00
#define WM_LR_PHASE_NORM_I2S 0x00
#define WM_LR_PHASE_REV_I2S 0x10
#define WM_LRSWAP_NORM 0x00
#define WM_LRSWAP_REV 0x20
#define WM_MASTER_MODE 0x40
#define WM_SLAVE_MODE 0x00
#define WM_BCLK_NORM 0x00
#define WM_BCLK_INV 0x80
// sampling control
#define WM_USB_MODE 0x01
#define WM_NORMAL_MODE 0x00
#define WM_OVERSAMPLE_RATE_384 0x02
#define WM_OVERSAMPLE_RATE_256 0x00
#define WM_CLKIDIV2_ON 0x40
#define WM_CLKIDIV2_OFF 0x00
#define WM_CLKODIV2_ON 0x80
#define WM_CLKODIV2_OFF 0x00
// samplerate settings for codec
#define WM8731_SR_32 0
#define WM8731_SR_44_1 1
#define WM8731_SR_48 2
#define WM8731_SR_88_2 3
#define WM8731_SR_96 4
// active control
#define WM_ACTIVE_ON 0x01
#define WM_ACTIVE_OFF 0x00

// initialize the WM8731 control driver
void WM8731_ctrl_init(void);

// start the WM8731 codec - set for normal operation
void WM8731_ctrl_start(void);

// set samplerate
void WM8731_ctrl_set_samplerate(int rate);

#endif