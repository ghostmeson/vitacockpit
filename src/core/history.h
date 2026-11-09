#ifndef COCKPIT_CORE_HISTORY_H
#define COCKPIT_CORE_HISTORY_H

/*
 * A fixed-capacity ring of samples, oldest evicted first.
 *
 * Deliberately dependency-free: it is a container, not domain data, which is
 * why the rendering layer is allowed to consume it directly for sparklines.
 */

#define HISTORY_CAP 200

typedef struct {
    float v[HISTORY_CAP];
    int head; /* index of the next write */
    int len;  /* number of valid samples, <= HISTORY_CAP */
} History;

void history_reset(History *h);
void history_push(History *h, float value);

/* i == 0 is the newest sample. Out-of-range returns 0. */
float history_at(const History *h, int i);

#endif
