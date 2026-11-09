#ifndef COCKPIT_UI_LAUNCH_CARD_H
#define COCKPIT_UI_LAUNCH_CARD_H

/*
 * The launch safety card's copy.
 *
 * Pure policy: it reads the editor's launch result and returns a title and a
 * body. It includes no gfx, no psp2 header, and no panel, so the host tests and
 * the preview can assert every branch. The surface that draws it lives in
 * ui/modal.
 */

#define LAUNCH_CARD_TITLE_MAX 32
#define LAUNCH_CARD_BODY_MAX 512

typedef struct {
    char title[LAUNCH_CARD_TITLE_MAX];
    char body[LAUNCH_CARD_BODY_MAX];
} LaunchCard;

void launch_card_build(LaunchCard *out);

#endif
