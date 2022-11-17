#ifndef _KEY_ASSIGN_H_
#define _KEY_ASSIGN_H_

#include <stdint.h>
#include <zephyr/kernel.h>

typedef void(*key_play_cb)(int index, int note);
typedef void(*key_stop_cb)(int index);

struct key_state {
    uint8_t index;
    uint8_t note;
    // velocity

    struct key_state* next;
};

struct keys {
    struct key_state keys[CONFIG_MAX_NOTES];
    struct key_state* active_head;
    struct key_state* inactive_head;
    const key_play_cb play_cb;
    const key_stop_cb stop_cb;

    struct k_mutex mutex;
};


void keys_init(struct keys* keys, key_play_cb play_cb, key_stop_cb stop_cb);

void keys_play(struct keys* keys, int note);
void keys_stop(struct keys* keys, int note);

void keys_print(struct keys* keys);

#endif