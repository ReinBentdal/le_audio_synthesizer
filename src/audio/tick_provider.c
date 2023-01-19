#include "tick_provider.h"

#include <stdlib.h>

#include <zephyr/kernel.h>
#include "audio_process.h"


static struct tick_provider_subscriber *_subscription_head = NULL;

static uint32_t _phase_accumulate;
static uint32_t _phase_increment;

void tick_provider_init(void)
{
    _phase_accumulate = 0;
    _phase_increment = 0;
}

void tick_provider_subscribe(struct tick_provider_subscriber* subscriber, tick_provider_notify_cb notifier)
{
    __ASSERT_NO_MSG(subscriber != NULL);
    __ASSERT_NO_MSG(notifier != NULL);
    __ASSERT(subscriber->next == NULL, "next item of tick provider subscriber is already set");

    subscriber->next = _subscription_head;
    subscriber->notifier = notifier;

    _subscription_head = subscriber;
}

void tick_provider_unsubscribe(struct tick_provider_subscriber* subscriber)
{
    __ASSERT_NO_MSG(subscriber != NULL);
    __ASSERT_NO_MSG(_subscription_head != NULL);

    struct tick_provider_subscriber** current = &_subscription_head;

    while (*current != subscriber) {
        current = &(*current)->next;
        __ASSERT(*current != NULL, "reached end of list without finding item");
    }

    *current = (*current)->next;
}

void tick_provider_set_bpm(uint32_t bpm)
{
    _phase_increment = (uint32_t)(bpm * PULSES_PER_QUARTER_NOTE * AUDIO_BLOCK_SIZE / (double)(60 * CONFIG_AUDIO_SAMPLE_RATE_HZ) * UINT32_MAX);
}

void tick_provider_increment(void)
{
    const uint32_t last_phase = _phase_accumulate;
    _phase_accumulate += _phase_increment;
    if (_phase_accumulate < last_phase)
    { /* overflow, update subscribers */
        struct tick_provider_subscriber *p = _subscription_head;
        while (p != NULL)
        {
            /* assumes callback is not null => NULL expception handled in subscription */
            p->notifier();
            p = p->next;
        }
    }
}