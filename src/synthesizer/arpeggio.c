#include "arpeggio.h"

#include <zephyr/kernel.h>
#include "tick_provider.h"

#define MAX_ACTIVE_NOTES 5

static int _tick_count = 0;

static struct keys _keys;

static int _notes[MAX_ACTIVE_NOTES] = {0};
static int _notes_active_length = 0;

static int _arp_current_note = 0;
static int _arp_next_note_idx = 0;

struct k_mutex _mutex;

static void _remove_first_note(void);

void arpeggio_init(key_play_cb play_cb, key_stop_cb stop_cb) {
    k_mutex_init(&_mutex);

    keys_init(&_keys, play_cb, stop_cb);
}

void arpeggio_note_add(int note) {
    k_mutex_lock(&_mutex, K_FOREVER);

    if (_notes_active_length == MAX_ACTIVE_NOTES-1) {
        _remove_first_note();
        _notes_active_length--;
    }

    _notes[_notes_active_length] = note;
    _notes_active_length++;

    k_mutex_unlock(&_mutex);
}

void arpeggio_note_remove(int note) {
    k_mutex_lock(&_mutex, K_FOREVER);

    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (_notes[i] == note) {
            _remove_note(i);
            _notes_active_length--;
            
            /* if removed note was last and next arp note was last, the next arp note has to loop back to first note */
            if (_arp_next_note_idx == _notes_active_length) {
                _arp_next_note_idx = 0;
            } 
            
            /* decrement next note if next note was shifted backwards */
            else if (_arp_next_note_idx > i) {
                _arp_next_note_idx--;
            }

            break;
        }
    }

    k_mutex_unlock(&_mutex);
}

void arpeggio_tick(void) {
    _tick_count++;
    if (_tick_count == PULSES_PER_QUARTER_NOTE) {
        _tick_count = 0;

        k_mutex_lock(&_mutex, K_FOREVER);

        /* stop last played note */
        if (_arp_current_note != 0) {
            keys_stop(&_keys, _arp_current_note);
        }

        if (_notes_active_length == 0) {
            _arp_current_note = 0;
        } else {
            /* start next note */
            _arp_current_note = _notes[_arp_next_note_idx];
            keys_play(&_keys, _arp_current_note);

            _arp_next_note_idx++;
            if (_arp_next_note_idx == MAX_ACTIVE_NOTES) {
                _arp_next_note_idx = 0;
            }
        }

        k_mutex_unlock(&_mutex);
    }
}

static void _remove_note(uint32_t index) {
    __ASSERT(index < MAX_ACTIVE_NOTES, "index out of range");

    for (int i = index + 1; i < MAX_ACTIVE_NOTES; i++) {
        _notes[i-1] =  _notes[i];
    }
    _notes[MAX_ACTIVE_NOTES-1] = 0;
}

static void _remove_first_note(void) {
    _remove_note(0);
}