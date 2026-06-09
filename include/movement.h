#ifndef GBA_MOVEMENT_H
#define GBA_MOVEMENT_H

#include <stdbool.h>
#include <stdint.h>

// Pure, host-testable logic for GB-Studio-style "movement types": per-actor
// movement patterns the engine drives every frame without a script running,
// the same convention GB Studio uses (actors are either script-driven,
// player-driven, or assigned a pattern like "patrol" or "follow").
//
// These functions only compute a velocity (and, for patrol, update direction
// state); they never touch actor position, collision, or hardware — that
// happens in engine_update via collision_resolve_movement, same as
// player-driven movement.

// Paces an actor back and forth across a rectangular bounds region at a
// constant speed, reversing direction when it reaches either edge.
//
// The bounds rectangle's *longer* axis determines the patrol axis: a wider
// rectangle (bounds_w >= bounds_h) patrols horizontally, a taller one
// patrols vertically. A degenerate bounds (both dimensions zero, or the
// actor already outside it on the patrol axis) leaves the actor stationary.
//
// `inout_moving_positive` is the actor's persisted direction flag (true =
// heading toward the bounds' max edge, i.e. right or down): it is read to
// decide which way to continue, and written back when an edge is reached so
// the next frame continues the bounce. Pass speed >= 0; this function never
// negates it itself — direction comes entirely from the flag.
//
// Output velocities are written to *out_vel_x / *out_vel_y (the other axis
// is always zero — patrol never moves diagonally).
void movement_patrol(int16_t actor_x, int16_t actor_y, int16_t bounds_x,
                      int16_t bounds_y, uint16_t bounds_w, uint16_t bounds_h,
                      int16_t speed, bool *inout_moving_positive,
                      int16_t *out_vel_x, int16_t *out_vel_y);

// Walks an actor toward (target_x, target_y) — typically the player — at up
// to `speed` pixels/frame on each axis independently (so diagonal approaches
// move on both axes at once, GB-Studio "follow player" style; this avoids
// any sqrt/normalisation, which the GBA's integer-only ARM7TDJI would pay
// dearly for every frame).
//
// The actor only moves while within `range` pixels of the target on *both*
// axes (a square aggro zone, Chebyshev distance) — outside that range it
// stays put, so e.g. an NPC only gives chase once the player wanders close.
// On each axis the step is clamped to the remaining distance so the actor
// settles on the target instead of jittering past it and back.
void movement_follow(int16_t actor_x, int16_t actor_y, int16_t target_x,
                      int16_t target_y, int16_t speed, uint16_t range,
                      int16_t *out_vel_x, int16_t *out_vel_y);

#endif
