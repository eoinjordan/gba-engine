#ifndef TEXTBOX_H
#define TEXTBOX_H

#include <stdbool.h>

// Textbox inner dimensions (tiles / characters).
#define TEXTBOX_CHARS_W 28
#define TEXTBOX_LINES   2

// Show a dialogue textbox at the bottom of the screen, render `text` into it
// (variable substitution + word-wrap applied), and arm the blocking wait.
// Call this from a VM_OP_SHOW_TEXT dispatch after formatting the string.
void textbox_open(const char *text);

// Called every frame while the textbox is blocking a script context.
// Returns true once the player presses A to dismiss, false while waiting.
// The VM runner clears ctx->update_fn and resumes when this returns true.
bool textbox_update(void);

// Hide the textbox and disable BG1. Called automatically by textbox_update
// on dismiss; exposed for forced teardown (e.g. scene transition).
void textbox_close(void);

// True while dialogue is visible. Gameplay input uses this to consume the
// keypress that dismisses a textbox instead of reusing it for movement or a
// second actor interaction in the same frame.
bool textbox_is_open(void);

// One-time hardware init: loads the embedded font into VRAM charblock 1 and
// writes the textbox palette into palette bank 15. Call from engine_init().
void textbox_init(void);

#endif
