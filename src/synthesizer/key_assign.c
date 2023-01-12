#include "key_assign.h"

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(keys, CONFIG_LOG_BUTTON_LEVEL);

// TODO: rewrite to only use one linked list, sorted by newest played note. Alternatively switch to using arrays

void keys_init(struct keys* keys, key_play_cb play_cb, key_stop_cb stop_cb) {
    __ASSERT(keys != NULL, "NULL pointer parameter");
    __ASSERT(play_cb != NULL, "play_cb must not be NULL");
    __ASSERT(stop_cb != NULL, "stop_cb must not be NULL");

    k_mutex_init(&keys->mutex);

    /* inserts all keys in the unused linked list */
    keys->head = NULL;
    *(key_play_cb*)&keys->play_cb = play_cb;
    *(key_stop_cb*)&keys->stop_cb = stop_cb;

    for (int i = 0; i < CONFIG_MAX_NOTES; i++) {
        keys->_keys[i].index = i;

        keys->_keys[i].next = keys->head;
        keys->head = &keys->_keys[i];
    }
}

void keys_play(struct keys* keys, int note) {
    __ASSERT(keys != NULL, "NULL pointer parameter");

    k_mutex_lock(&keys->mutex, K_FOREVER);

    /* use last active note => last item in list */
    struct key** key_indirect = &keys->head;

    while ((*key_indirect)->next != NULL) {
        key_indirect = &(*key_indirect)->next;
    }

    struct key* key = *key_indirect;
    *key_indirect = NULL;

    key->next = keys->head;
    keys->head = key;

    key->note = note;

    k_mutex_unlock(&keys->mutex);

    keys->play_cb(key->index, key->note);
}

void keys_stop(struct keys* keys, int note) {
    __ASSERT(keys != NULL, "NULL pointer parameter");

    k_mutex_lock(&keys->mutex, K_FOREVER);

    /* iterate through and move found key to after active keys */
    struct key** key_indirect = &keys->head;

    while ((*key_indirect)->note != note) {
        key_indirect = &(*key_indirect)->next;

        if ((*key_indirect) == NULL || (*key_indirect)->note == 0) {
            LOG_WRN("tried to stop note which was not already playing: %d", note);
            k_mutex_unlock(&keys->mutex);
            return;
        }
    }

    /* remove key from list */
    struct key* key = *key_indirect;
    *key_indirect = key->next;

    /* continue iterating until the next note is inactive or end of list */
    while (*key_indirect != NULL && (*key_indirect)->note != 0) {
        key_indirect = &(*key_indirect)->next;
    }

    /* insert key again in list */
    key->next = *key_indirect;
    *key_indirect = key;

    key->note = 0;

    k_mutex_unlock(&keys->mutex);

    keys->stop_cb(key->index);
}

void keys_print(struct keys* keys) {
    __ASSERT(keys != NULL, "NULL pointer parameter");
    struct key* key;

    LOG_INF("--keys--");
    key = keys->head;
    while(key != NULL) {
        LOG_INF("index %d, note %d", key->index, key->note);
        key = key->next;
    }
}