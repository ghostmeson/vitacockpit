#ifndef COCKPIT_UI_ABOUT_TEXT_H
#define COCKPIT_UI_ABOUT_TEXT_H

#include <stddef.h>

/*
 * The ABOUT reboot confirmation copy. Pure policy: it reads the editor's
 * pending state and returns the title and body. ABOUT never writes `config.txt`,
 * so the confirmation labels its own discard action instead of echoing the
 * dock's apply-and-reboot labels.
 */
void about_reboot_text(char *title, size_t title_cap, char *body,
                       size_t body_cap);

#endif
