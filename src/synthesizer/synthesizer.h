#ifndef _SYNTHESIZER_H_
#define _SYNTHESIZER_H_

#include "button.h"

void synthesizer_init();

void synthesizer_key_event(struct button_event*);

/* returns false if nothing was processed */
bool synthesizer_process(int8_t* block, const size_t block_size);

#endif