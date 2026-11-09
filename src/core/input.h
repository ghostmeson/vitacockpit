#ifndef COCKPIT_CORE_INPUT_H
#define COCKPIT_CORE_INPUT_H

/*
 * Pad edges, repeats, and analog scroll, independent of the Vita SDK.
 *
 * The old loop integrated a held direction every frame, which is fine for a
 * scroll but wrong for a tab or a toggle: holding Cross would write sixty times
 * a second, and a modal would accept its own opening press. This module turns
 * the pad into at most one event per frame and owns the repeat cadence.
 *
 * Continuous scrolling is separate from those events (KTD4, R45-R48): the app
 * applies `scroll_y` to the active surface, while the D-pad keeps its edge and
 * repeat events. Modals keep the D-pad for choices and ignore the stick.
 */

typedef enum {
    INPUT_NONE = 0,
    INPUT_TAB_LEFT,
    INPUT_TAB_RIGHT,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_ACCEPT,
    INPUT_CANCEL,
    INPUT_OPTIONS
} InputEvent;

typedef enum {
    INPUT_CONTEXT_APP = 0,
    INPUT_CONTEXT_MODAL,
} InputContext;

typedef struct {
    int up;
    int down;
    int left;
    int right;
    int tab_left;
    int tab_right;
    int accept;
    int cancel;
    int options;
    float repeat;
    float scroll_y; /* px/s, already dead-zoned and curved; 0 in a modal */
} InputState;

#define INPUT_REPEAT_DELAY 0.42f
#define INPUT_REPEAT_RATE 0.09f

/* Stick scroll policy (R45). The dead zone is on the normalized axis. */
#define INPUT_STICK_DEAD_ZONE 0.18f
#define INPUT_SCROLL_SPEED 520.0f

void input_reset(InputState *state);

/*
 * The larger vertical deflection of the two sticks, signed. Reading both and
 * passing one keeps two held sticks from adding to double speed.
 */
float input_stronger_stick(float left_y, float right_y);

/*
 * `context` selects what the pad means: the app, or the error interrupt
 * (accept/cancel only). The opening press is already in the previous state, so
 * the modal cannot accept itself next frame. `stick_y` is the normalized
 * vertical axis, positive down; a modal ignores it.
 */
InputEvent input_update(InputState *state, int up, int down, int left, int right,
                        int accept, int cancel, int options, int tab_left,
                        int tab_right, float stick_y, InputContext context,
                        float dt);

#endif
