#ifndef _ARPEGGIO_H_
#define _ARPEGGIO_H_

#include <stdint.h>

#include "key_assign.h"

void arpeggio_init(key_play_cb play_cb, key_stop_cb stop_cb);

void arpeggio_note_add(int note);
void arpeggio_note_remove(int note);

void arpeggio_tick(void);

void arpeggio_set_divider(uint32_t divider);

#endif