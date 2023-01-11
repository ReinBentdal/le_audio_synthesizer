#include "arpeggio.h"

#include <zephyr/kernel.h>
#include "tick_provider.h"

int _tick_count = 0;

struct keys _keys;

#define MAX_ACTIVE_NOTES 5

struct note_item {
    int note;
    struct note_item* next;
};

struct note_item _notes[MAX_ACTIVE_NOTES];

/* sorted such that last played is first */
struct note_item* _notes_head = NULL;

struct note_item* _playing_note = NULL;
int _last_played_note = 0;

struct k_mutex _mutex;

void arpeggio_init(key_play_cb play_cb, key_stop_cb stop_cb) {
    k_mutex_init(&_mutex);

    keys_init(&_keys, play_cb, stop_cb);

    for (int i = 0; MAX_ACTIVE_NOTES; i++) {
        _notes[i].note = 0;
        _notes[i].next = _notes_head;
        _notes_head = &_notes[i];
    }
}

void arpeggio_play(int note) {

    k_mutex_lock(&_mutex, K_FOREVER);
    /* use last element i linked list */
    struct note_item** p_indirect = &_notes_head;

    /* iterate until til indirect points to the last element */
    while ((*p_indirect)->next != NULL) {
        p_indirect = &(*p_indirect)->next;
    }
    
    struct note_item* last = *p_indirect;
    last->note = note;

    /* move to fron of list */
    last->next = _notes_head;
    _notes_head = last;

    *p_indirect = NULL;

    k_mutex_unlock(&_mutex);
}

void arpeggio_stop(int note) {
    struct note_item** p_indirect = &_notes_head;

    while ((*p_indirect)->note != note && (*p_indirect)->note != 0) {
        p_indirect = &(*p_indirect)->next;
    }

    if ((*p_indirect)->note == 0) {
        LOG_WRN("did not find note to stop in arpeggio");
        return;
    }

    struct note_item* found = *p_indirect;
    found->note = 0;
    *p_indirect = found->next;

    /* move to first of unused notes */
    while ((*p_indirect)->next != NULL && (*p_indirect)->next->note != 0) {
        p_indirect = &(*p_indirect)->next;
    }

    found->next = *p_indirect;
    *p_indirect = &found;
}

void arpeggio_tick(void) {
    _tick_count++;
    if (_tick_count == PULSES_PER_QUARTER_NOTE) {
        _tick_count = 0;


    }
}