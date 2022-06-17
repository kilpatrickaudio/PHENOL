/*
 * USB Controller - Handles USB Setup and Callbacks
 *
 * Copyright 2012: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef USB_CTRL_H
#define USB_CTRL_H

// MIDI ports - specific to this application
#define MIDI_PORT_USB 0

// init the USB
void usb_ctrl_init(void);

// poll the USB and handle transactions - call this at least every 1ms
void usb_ctrl_poll(void);

// check to see if the USB is enabled - 0 = disabled, 1 = enabled
unsigned char usb_ctrl_get_enabled(void);

#endif

