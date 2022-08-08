#include "key_assign.h"

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(keys, CONFIG_LOG_BUTTON_LEVEL);

void keys_init(struct keys* keys, key_play_cb play_cb, key_stop_cb stop_cb) {
    __ASSERT(keys != NULL, "NULL pointer parameter");

    k_mutex_init(&keys->mutex);

    /* inserts all keys in the unused linked list */
    keys->active_head = NULL;
    keys->inactive_head = NULL;
    keys->play_cb = play_cb;
    keys->stop_cb = stop_cb;

    for (int i = 0; i < CONFIG_MAX_NOTES; i++) {
        keys->keys[i].index = i;
        keys->keys[i].next = keys->inactive_head;
        keys->inactive_head = &keys->keys[i];
    }
}

void keys_play(struct keys* keys, int note) {
    __ASSERT(keys != NULL, "NULL pointer parameter");

    k_mutex_lock(&keys->mutex, K_FOREVER);

    struct key_state* key;
    
    /* use inactive key which is the longest sinced been in use */
    if (keys->inactive_head != NULL) {
        __ASSERT(keys->inactive_head != NULL, "both inactive and active head cannot be null");
        key = keys->inactive_head;
        struct key_state* prev = NULL;
        while(key->next != NULL) {
            prev = key;
            key = key->next;
        }

        if (prev != NULL) {
            prev->next = NULL;
        } else {
            keys->inactive_head = NULL;
        }
    }

    /* remove active key which has played the longest */
    else {
        __ASSERT(keys->active_head != NULL, "both inactive and active head cannot be null");
        key = keys->active_head;
        struct key_state* prev = NULL;
        while(key->next != NULL) {
            prev = key;
            key = key->next;
        }
        __ASSERT(prev != NULL, "there must be more than 2 elements in the active head if there are non in the inactive list");
        prev->next = NULL;
    }
    // TODO: explicit stop note?

    key->note = note;

    key->next = keys->active_head;
    keys->active_head = key;

    __ASSERT(keys->play_cb != NULL, "cb function is a NULL ptr");
    keys->play_cb(key->index, key->note);
    
    k_mutex_unlock(&keys->mutex);
}

void keys_stop(struct keys* keys, int note) {
    __ASSERT(keys != NULL, "NULL pointer parameter");

    k_mutex_lock(&keys->mutex, K_FOREVER);

    struct key_state* key = keys->active_head;
    struct key_state* prev = NULL;
    while(key != NULL) {
        if (key->note == note) {
            break;
        }
        prev = key;
        key = key->next;
    }

    if (key == NULL) {
        LOG_WRN("tried stopping note which was not playing: %d", note);
        return;
    }

    if (prev != NULL) {
        prev->next = key->next;
    } else {
        keys->active_head = key->next;
    }

    key->next = keys->inactive_head;
    keys->inactive_head = key;

    __ASSERT(keys->stop_cb != NULL, "cb function is a NULL ptr");
    keys->stop_cb(key->index);

    k_mutex_unlock(&keys->mutex);
}

void keys_print(struct keys* keys) {
    __ASSERT(keys != NULL, "NULL pointer parameter");
    struct key_state* key;

    LOG_INF("--pressed--");
    key = keys->active_head;
    while(key != NULL) {
        LOG_INF("index %d, note %d", key->index, key->note);
        key = key->next;
    }

    LOG_INF("--inactive");
    key = keys->inactive_head;
    while(key != NULL) {
        LOG_INF("index %d", key->index);
        key = key->next;
    }
}