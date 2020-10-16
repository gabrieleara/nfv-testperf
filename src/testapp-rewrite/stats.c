#include "stats.h"

#define STATS_SIZE 64
#define STATS_INDEX_MASK 0x3F

const struct stats STATS_INIT = {
    .type = STATS_TX, .first = 0, .last = 0, .count = 0};

void stats_save(struct stats *s, union stats_data *d) {
    if (s->count < STATS_SIZE) {
        // Fill up, s->first will remain equal to zero until filled
        s->data[s->last] = *d;
        ++s->count;

        if (s->count < STATS_SIZE)
            s->last = (s->last + 1) & STATS_INDEX_MASK;
    } else {
        // Substitute last value
        s->data[s->first] = *d;
        s->last = s->first;
        s->first = (s->first + 1) & STATS_INDEX_MASK;
    }
}

void stats_print_all(struct stats *s) {
    if (!s->count) {
        // atomic_flag_clear(&s->in_use);

        printf("No stats to be printed.\n");
        return;
    }

    for (int i = s->first; i != s->last; i = (i + 1) & STATS_INDEX_MASK) {
        stats_print(s->type, &s->data[i]);
    }

    if (s->count == STATS_SIZE) {
        stats_print(s->type, &s->data[s->last]);
    }
}
