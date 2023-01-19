#include "arpeggio.h"

#include <zephyr/kernel.h>
#include "tick_provider.h"

#define MAX_ACTIVE_NOTES 5

static int _tick_count = 0;

static struct keys _keys;

static int _notes[MAX_ACTIVE_NOTES] = {0};
static int _notes_active_length = 0;

static int _arp_current_note = 0;
static int _arp_current_note_idx = 0;

static struct k_mutex _mutex;

static bool _arp_enabled = false;

#define MAX_OCTAVES 3
static uint32_t _arp_current_octave = 0;

static uint32_t _divider;

static void _remove_note(uint32_t index);
static void _remove_first_note(void);

void arpeggio_init(key_play_cb play_cb, key_stop_cb stop_cb) {
    __ASSERT_NO_MSG(play_cb != NULL);
    __ASSERT_NO_MSG(stop_cb != NULL);
    
    k_mutex_init(&_mutex);

    keys_init(&_keys, play_cb, stop_cb);

    _divider = PULSES_PER_QUARTER_NOTE;
}

void arpeggio_note_add(int note) {
    k_mutex_lock(&_mutex, K_FOREVER);

    if (_notes_active_length == MAX_ACTIVE_NOTES) {
        _remove_first_note();
        _notes_active_length--;
    }

    _notes[_notes_active_length] = note;
    _notes_active_length++;

    /* restart arpeggio if first note */
    if (_arp_enabled == false) {
        _tick_count = 0;
        _arp_enabled = true;
        _arp_current_octave = 0;
    }

    k_mutex_unlock(&_mutex);
}

void arpeggio_note_remove(int note) {
    k_mutex_lock(&_mutex, K_FOREVER);

    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (_notes[i] == note) {
            _remove_note(i);
            _notes_active_length--;

            /* if removed note was last and current arp note was last, the next arp note has to loop back to first note */
            if (_arp_current_note_idx == _notes_active_length) {
                _arp_current_note_idx = 0;
            }
            
            /* decrement next note if next note was shifted backwards */
            else if (_arp_current_note_idx > i) {
                _arp_current_note_idx--;
            }

            break;
        }
    }

    k_mutex_unlock(&_mutex);
}

void arpeggio_tick(void) {
    if (_arp_enabled == false) return;

    if (_tick_count == 0) {

        /* stop last played note */
        if (_arp_current_note != 0) {
            keys_stop(&_keys, _arp_current_note);
        }

        k_mutex_lock(&_mutex, K_FOREVER);

        /* dont play new note if there are no active notes */
        if (_notes_active_length == 0) {
            _arp_enabled = false;
            _arp_current_note = 0;
        } 
        
        /* play next active note */
        else {

            /* increment to next note to play */
            _arp_current_note_idx++;
            if (_arp_current_note_idx == _notes_active_length) {
                _arp_current_note_idx = 0;

                _arp_current_octave++;
                if (_arp_current_octave == MAX_OCTAVES) {
                    _arp_current_octave = 0;
                }
            }
            _arp_current_note = _notes[_arp_current_note_idx] + 12*_arp_current_octave;
            
            keys_play(&_keys, _arp_current_note);
        }

        k_mutex_unlock(&_mutex);
    }

    _tick_count++;
    if (_tick_count == _divider) {
        _tick_count = 0;
    }
}

void arpeggio_set_divider(uint32_t divider) {
    __ASSERT_NO_MSG(divider != 0);

    _divider = divider;

    /* make sure tick count is not larger than divider */
    _tick_count = 0;
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