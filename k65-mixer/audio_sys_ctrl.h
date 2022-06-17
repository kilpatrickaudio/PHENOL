/*
 * KSEND Audio Node - Device Control Functions
 *
 * Copyright 2013: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef AUDIO_SYS_CTRL_H
#define AUDIO_SYS_CTRL_H

// write to the i2c control bus for audio devices
void audio_sys_write_ctrl(unsigned char addr, 
		unsigned char tx_data[], int tx_data_len);

// read from the i2c control bus for audio devices
void audio_sys_read_ctrl(unsigned char addr,
		unsigned char tx_data[], int tx_data_len,
		unsigned char rx_data[], int rx_data_len);

#endif
