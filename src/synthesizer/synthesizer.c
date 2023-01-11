
#include "zephyr/kernel.h"
#include "synthesizer.h"

#include "arpeggio.h"
#include "instrument.h"

struct instrument _instrument;

#define NOTE_BASE 40
static const uint8_t key_map[] = {NOTE_BASE+12, NOTE_BASE+14, NOTE_BASE+18, NOTE_BASE+19, NOTE_BASE+21};

static inline void _key_play_cb(int index, int note);
static inline void _key_stop_cb(int index);

void synthesizer_init() {
    arpeggio_init(_key_play_cb, _key_stop_cb);
    instrument_init(&_instrument);
}

void synthesizer_key_event(struct button_event* button_event) {
    __ASSERT_NO_MSG(button_event != NULL);
    
    switch (button_event->state) {
        case BUTTON_PRESSED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");
            arpeggio_note_add(key_map[button_event->index]);
            break;
        }
        case BUTTON_RELEASED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");
            arpeggio_note_remove(key_map[button_event->index]);
            break;
        }
    }
}

bool synthesizer_process(int8_t* block, const size_t block_size) {
    return instrument_process(&_instrument, block, block_size);
}

void synthesizer_tick(void) {
    arpeggio_tick();
}


static inline void _key_play_cb(int index, int note) {
	instrument_play_note(&_instrument, index, note);
}

static inline void _key_stop_cb(int index) {
	instrument_stop_note(&_instrument, index);
}
