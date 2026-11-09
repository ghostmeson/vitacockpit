#include "core/input.h"

#include <string.h>

void input_reset(InputState *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

static int pressed(int now, int before)
{
    return now && !before;
}

/*
 * Linear after the dead zone, then squared: small deflections get fine control,
 * the rim reaches the full 520 px/s. The sign survives the curve.
 */
static float scroll_speed(float stick_y)
{
    float magnitude = stick_y < 0.0f ? -stick_y : stick_y;

    if (magnitude <= INPUT_STICK_DEAD_ZONE)
        return 0.0f;

    if (magnitude > 1.0f)
        magnitude = 1.0f;

    float travel = (magnitude - INPUT_STICK_DEAD_ZONE) /
                   (1.0f - INPUT_STICK_DEAD_ZONE);

    travel *= travel;

    return stick_y < 0.0f ? -travel * INPUT_SCROLL_SPEED
                          : travel * INPUT_SCROLL_SPEED;
}

float input_stronger_stick(float left_y, float right_y)
{
    float left_mag = left_y < 0.0f ? -left_y : left_y;
    float right_mag = right_y < 0.0f ? -right_y : right_y;

    return right_mag > left_mag ? right_y : left_y;
}

static InputEvent repeat_direction(InputState *state, int up, int down, float dt)
{
    if (pressed(up, state->up)) {
        state->repeat = 0.0f;
        return INPUT_UP;
    }

    if (pressed(down, state->down)) {
        state->repeat = 0.0f;
        return INPUT_DOWN;
    }

    if (up || down) {
        state->repeat += dt;
        if (state->repeat >= INPUT_REPEAT_DELAY) {
            state->repeat = INPUT_REPEAT_DELAY - INPUT_REPEAT_RATE;
            return up ? INPUT_UP : INPUT_DOWN;
        }

        return INPUT_NONE;
    }

    state->repeat = 0.0f;
    return INPUT_NONE;
}

InputEvent input_update(InputState *state, int up, int down, int left, int right,
                        int accept, int cancel, int options, int tab_left,
                        int tab_right, float stick_y, InputContext context,
                        float dt)
{
    if (state == NULL)
        return INPUT_NONE;

    InputEvent event = INPUT_NONE;

    /* The stick is continuous input, not an event, so it is sampled even on a
     * frame that also produces a D-pad edge. The error modal consumes only
     * accept/cancel and has no scroll. */
    state->scroll_y =
        context == INPUT_CONTEXT_APP ? scroll_speed(stick_y) : 0.0f;

    if (context != INPUT_CONTEXT_APP) {
        if (pressed(accept, state->accept)) {
            event = INPUT_ACCEPT;
        } else if (pressed(cancel, state->cancel)) {
            event = INPUT_CANCEL;
        } else {
            state->repeat = 0.0f;
        }
    } else if (pressed(tab_left, state->tab_left)) {
        event = INPUT_TAB_LEFT;
    } else if (pressed(tab_right, state->tab_right)) {
        event = INPUT_TAB_RIGHT;
    } else if (pressed(accept, state->accept)) {
        event = INPUT_ACCEPT;
    } else if (pressed(cancel, state->cancel)) {
        event = INPUT_CANCEL;
    } else if (pressed(options, state->options)) {
        event = INPUT_OPTIONS;
    } else if (pressed(left, state->left)) {
        event = INPUT_LEFT;
    } else if (pressed(right, state->right)) {
        event = INPUT_RIGHT;
    } else {
        event = repeat_direction(state, up, down, dt);
    }

    state->up = up;
    state->down = down;
    state->left = left;
    state->right = right;
    state->tab_left = tab_left;
    state->tab_right = tab_right;
    state->accept = accept;
    state->cancel = cancel;
    state->options = options;

    return event;
}
