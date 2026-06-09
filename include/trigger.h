#ifndef GBA_TRIGGER_H
#define GBA_TRIGGER_H

#include <stdbool.h>
#include <stdint.h>

// Pure, host-testable geometry for scene triggers: rectangular zones that
// run a script when an actor (almost always the player) steps into them —
// GB Studio's "trigger" event source. The engine owns deciding *when* to
// check (each frame, after movement resolves) and *what* to do on overlap
// (run the trigger's script via the VM); this module only answers "is this
// actor's bounding box currently inside this zone?".
//
// Both rectangles are axis-aligned boxes in the same coordinate space
// (pixels, matching actor_t::x/y and bounds_w/h, and the trigger zone's
// compiled position/size — whatever units the compiler emits, as long as
// both sides agree). A rectangle with zero width or height has no area and
// can never overlap anything, including another zero-area rectangle at the
// same position — this matches the intuitive "an empty trigger never fires"
// behaviour and avoids degenerate always-true comparisons at the edges.
bool trigger_rects_overlap(int16_t a_x, int16_t a_y, uint16_t a_w,
                           uint16_t a_h, int16_t b_x, int16_t b_y,
                           uint16_t b_w, uint16_t b_h);

// Scans `trigger_x`/`trigger_y`/`trigger_w`/`trigger_h` (parallel arrays of
// `trigger_count` zones — deliberately data-format-agnostic, since the
// compiled trigger layout isn't finalised yet) and returns the index of the
// first zone the actor's box overlaps, or TRIGGER_NONE if it overlaps none.
//
// Returns the *first* match (lowest index) so trigger ordering in compiled
// scene data is deterministic and meaningful — mirrors GB Studio, where
// overlapping triggers resolve in a stable, author-controlled order rather
// than randomly.
#define TRIGGER_NONE (-1)

int trigger_find_overlap(int16_t actor_x, int16_t actor_y, uint16_t actor_w,
                         uint16_t actor_h, const int16_t *trigger_x,
                         const int16_t *trigger_y, const uint16_t *trigger_w,
                         const uint16_t *trigger_h, uint8_t trigger_count);

#endif
