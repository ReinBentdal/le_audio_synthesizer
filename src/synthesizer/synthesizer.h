/**
 * @file synthesizer.h
 * @author Rein Gundersen Bentdal
 * @brief Glue module, synthesizer example using an arpeggio and a bank of oscillators with envelope and modulation
 * @version 0.1
 * @date 2023-01-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _SYNTHESIZER_H_
#define _SYNTHESIZER_H_

#include "button.h"
#include "tick_provider.h"

void synthesizer_init();

void synthesizer_key_event(struct button_event*);

/* returns false if nothing was processed */
bool synthesizer_process(int8_t* block, const size_t block_size);

void synthesizer_tick(void);

#endif