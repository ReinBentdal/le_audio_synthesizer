
#include "zephyr/kernel.h"
#include "synthesizer.h"

#include "key_assign.h"
#include "instrument.h"

struct instrument _instrument;
struct keys _keys;

#define NOTE_BASE 40
static const uint8_t key_map[] = {NOTE_BASE+12, NOTE_BASE+14, NOTE_BASE+18, NOTE_BASE+19, NOTE_BASE+21};

static inline void _key_play_cb(int index, int note);
static inline void _key_stop_cb(int index);

void synthesizer_init() {
    keys_init(&_keys, _key_play_cb, _key_stop_cb);
    instrument_init(&_instrument);
}

void synthesizer_key_event(struct button_event* button_event) {
    __ASSERT_NO_MSG(button_event != NULL);
    
    switch (button_event->state) {
        case BUTTON_PRESSED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");
            keys_play(&_keys, key_map[button_event->index]);
            break;
        }
        case BUTTON_RELEASED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");
            keys_stop(&_keys, key_map[button_event->index]);
            break;
        }
    }
}

/* returns false if nothing was processed */
bool synthesizer_process(int8_t* block, const size_t block_size) {
    return instrument_process(&_instrument, block, block_size);
}

void synthesizer_tick(void) {
    
}


static inline void _key_play_cb(int index, int note) {
	instrument_play_note(&_instrument, index, note);
}

static inline void _key_stop_cb(int index) {
	instrument_stop_note(&_instrument, index);
}
