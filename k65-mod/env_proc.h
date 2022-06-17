/*
 * K65 Phenol - Mod Processor Envelope / LFO Routines
 *
 * Copyright 2014: Andrew Kilpatrick
 * Written by: Andrew Kilpatrick
 *
 */
#ifndef ENV_PROC_H
#define ENV_PROC_H

// init the env proc
void env_proc_init(void);

// run the timer task
void env_proc_timer_task(void);

// run the sample task
void env_proc_sample_task(void);

#endif
