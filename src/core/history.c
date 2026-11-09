#include "core/history.h"

#include <string.h>

void history_reset(History *h)
{
    if (h == NULL)
        return;
    memset(h, 0, sizeof(*h));
}

void history_push(History *h, float value)
{
    if (h == NULL)
        return;

    h->v[h->head] = value;
    h->head = (h->head + 1) % HISTORY_CAP;

    if (h->len < HISTORY_CAP)
        ++h->len;
}

float history_at(const History *h, int i)
{
    if (h == NULL || i < 0 || i >= h->len)
        return 0.0f;

    int idx = h->head - 1 - i;
    while (idx < 0)
        idx += HISTORY_CAP;

    return h->v[idx];
}
