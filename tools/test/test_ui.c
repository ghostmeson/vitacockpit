#include <stdio.h>
#include <string.h>

#include "core/input.h"
#include "ui/core_text.h"
#include "ui/plugin_nav.h"
#include "ui/ui_state.h"

/*
 * Host tests for the PLUGINS input routing. No SDK, renderer, or panel is
 * linked; these are the rules the app and the preview must share.
 */

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__,      \
                    #cond);                                              \
        }                                                                \
    } while (0)

static void test_nav_modal(void)
{
    UiState ui;

    memset(&ui, 0, sizeof(ui));
    ui.tab = UI_TAB_PLUGINS;
    ui.modal = UI_MODAL_ERROR;

    /* ERROR is acknowledgement-only: Cross and Circle both close it. */
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) ==
          PLUGIN_NAV_MODAL_DISMISS);
    CHECK(plugin_nav_action(&ui, INPUT_CANCEL, 9, 0) ==
          PLUGIN_NAV_MODAL_DISMISS);
    CHECK(plugin_nav_action(&ui, INPUT_LEFT, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_RIGHT, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_OPTIONS, 9, 0) == PLUGIN_NAV_NONE);
}

static void test_nav_surfaces(void)
{
    UiState ui;

    memset(&ui, 0, sizeof(ui));
    ui.tab = UI_TAB_HOME;

    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) ==
          PLUGIN_NAV_HOME_SCROLL_UP);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_HOME_SCROLL_DOWN);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_TAB_LEFT, 9, 0) ==
          PLUGIN_NAV_TAB_TOGGLE);

    ui.tab = UI_TAB_PLUGINS;
    ui.focus = UI_FOCUS_LIST;
    ui.selection = 0;

    /* The first row has no surface above it. */
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_SELECT_DOWN);

    ui.selection = 1;
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) == PLUGIN_NAV_SELECT_UP);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_SELECT_DOWN);

    ui.selection = 8;
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_DOCK_ENTER);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) ==
          PLUGIN_NAV_ACTIVATE_ROW);
    CHECK(plugin_nav_action(&ui, INPUT_OPTIONS, 9, 0) ==
          PLUGIN_NAV_DOCK_TOGGLE);

    /* An empty ledger still has the dock below it. */
    ui.selection = -1;
    CHECK(plugin_nav_action(&ui, INPUT_UP, 0, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 0, 0) ==
          PLUGIN_NAV_DOCK_ENTER);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 0, 0) == PLUGIN_NAV_NONE);

    /* A preview owns the screen; Cross must not stage a row. */
    ui.selection = 0;
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 1) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 1) ==
          PLUGIN_NAV_DOCK_ENTER);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 1) == PLUGIN_NAV_NONE);

    ui.focus = UI_FOCUS_DOCK;
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) == PLUGIN_NAV_DOCK_LEAVE);
    CHECK(plugin_nav_action(&ui, INPUT_CANCEL, 9, 0) ==
          PLUGIN_NAV_DOCK_LEAVE);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_LEFT, 9, 0) == PLUGIN_NAV_DOCK_LEFT);
    CHECK(plugin_nav_action(&ui, INPUT_RIGHT, 9, 0) ==
          PLUGIN_NAV_DOCK_RIGHT);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) ==
          PLUGIN_NAV_ACTIVATE_DOCK);
    CHECK(plugin_nav_action(&ui, INPUT_OPTIONS, 9, 0) ==
          PLUGIN_NAV_DOCK_TOGGLE);
    CHECK(plugin_nav_action(&ui, INPUT_TAB_RIGHT, 9, 0) ==
          PLUGIN_NAV_TAB_TOGGLE);

    CHECK(plugin_nav_action(NULL, INPUT_UP, 9, 0) == PLUGIN_NAV_NONE);
}

/*
 * CORE's absence contract is display policy, not sampling policy: every cell
 * and both rate fields must render "--" independently. A zero would look like
 * a measurement.
 */
static void test_core_text(void)
{
    char buf[32];
    char left[32];
    char right[48];

    core_text_format_core(buf, sizeof(buf), 18, 1);
    CHECK(strcmp(buf, "18%") == 0);

    core_text_format_core(buf, sizeof(buf), 0, 0);
    CHECK(strcmp(buf, "--") == 0);

    core_text_format_caption(left, sizeof(left), right, sizeof(right),
                             1, 13, 1, 840, 1, 230);
    CHECK(strcmp(left, "CPU LOAD AVG 13%") == 0);
    CHECK(strcmp(right, "CTX 840/S  WAKE 230/S") == 0);

    /* No CPU sample: the average is unknown, but live rates may still show. */
    core_text_format_caption(left, sizeof(left), right, sizeof(right),
                             0, 0, 1, 840, 1, 230);
    CHECK(strcmp(left, "CPU LOAD AVG --") == 0);
    CHECK(strcmp(right, "CTX 840/S  WAKE 230/S") == 0);

    /* A frozen context counter must not print as 0/S. */
    core_text_format_caption(left, sizeof(left), right, sizeof(right),
                             1, 13, 0, 0, 1, 230);
    CHECK(strcmp(left, "CPU LOAD AVG 13%") == 0);
    CHECK(strcmp(right, "CTX --  WAKE 230/S") == 0);

    /* Both counters unavailable. */
    core_text_format_caption(left, sizeof(left), right, sizeof(right),
                             1, 13, 0, 0, 0, 0);
    CHECK(strcmp(left, "CPU LOAD AVG 13%") == 0);
    CHECK(strcmp(right, "CTX --  WAKE --") == 0);
}

int main(void)
{
    test_nav_modal();
    test_nav_surfaces();
    test_core_text();

    if (g_failures != 0) {
        fprintf(stderr, "%d checks, %d failure(s)\n", g_checks, g_failures);
        return 1;
    }

    printf("%d checks, 0 failure(s)\n", g_checks);
    return 0;
}
