// Unit tests for the bytecode script runner (src/vm.c) and other portable,
// hardware-free engine logic (e.g. src/camera.c).
//
// This code is plain, portable C with no hardware dependencies, so it can be
// compiled and exercised directly on the host. These tests cover context
// allocation/recycling, opcode dispatch, waiting, exception handling, and
// camera follow/clamp math — the parts of the engine most likely to harbour
// subtle logic bugs and least likely to need a real GBA (or emulator) to
// validate.

#include "camera.h"
#include "collision.h"
#include "movement.h"
#include "savegame.h"
#include "test_framework.h"
#include "test_stubs.h"
#include "text.h"
#include "trigger.h"
#include "vm.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void reset_vm(void) {
  stub_reset();
  script_runner_init(true);
}

// ---------------------------------------------------------------------------
// script_runner_init
// ---------------------------------------------------------------------------

TEST(init_starts_with_no_running_contexts) {
  reset_vm();
  ASSERT_EQ(script_runner_update(), RUNNER_IDLE);
}

TEST(init_clears_exception_state) {
  reset_vm();

  // Run a script that raises an exception, then re-init and confirm the
  // runner reports a clean idle state again (not a stale exception).
  static const uint8_t bad_script[] = {0xFF};
  script_execute(0, (uint8_t *)bad_script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_EXCEPTION);

  script_runner_init(true);
  ASSERT_EQ(script_runner_update(), RUNNER_IDLE);
}

// ---------------------------------------------------------------------------
// script_execute / context allocation
// ---------------------------------------------------------------------------

TEST(execute_returns_a_context_and_assigns_a_handle) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_END};
  uint16_t handle = 0;
  SCRIPT_CTX *ctx = script_execute(0, (uint8_t *)script, &handle, 0);

  ASSERT_NOT_NULL(ctx);
  ASSERT_EQ(handle, ctx->ID);
}

TEST(execute_pushes_arguments_onto_the_context_stack) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_END};
  SCRIPT_CTX *ctx =
      script_execute(0, (uint8_t *)script, NULL, 3, 10, 20, 30);

  ASSERT_NOT_NULL(ctx);
  // Arguments are pushed in order starting at base_addr.
  ASSERT_EQ(ctx->base_addr[0], 10);
  ASSERT_EQ(ctx->base_addr[1], 20);
  ASSERT_EQ(ctx->base_addr[2], 30);
  ASSERT_EQ(ctx->stack_ptr - ctx->base_addr, 3);
}

TEST(execute_runs_out_of_contexts_gracefully) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_WAIT, 0xFF};
  int allocated = 0;

  // VM_MAX_CONTEXTS is finite — exhaust the pool and confirm the runner
  // returns NULL rather than corrupting state once it's empty.
  for (int i = 0; i < VM_MAX_CONTEXTS; i++) {
    SCRIPT_CTX *ctx = script_execute(0, (uint8_t *)script, NULL, 0);
    if (ctx == NULL) {
      break;
    }
    allocated++;
  }

  ASSERT_EQ(allocated, VM_MAX_CONTEXTS);
  ASSERT_NULL(script_execute(0, (uint8_t *)script, NULL, 0));
}

TEST(terminated_contexts_are_returned_to_the_free_list) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_END};

  // Run VM_MAX_CONTEXTS scripts to completion (each immediately terminates),
  // then confirm a fresh batch can still be allocated — i.e. contexts are
  // recycled rather than leaked.
  for (int i = 0; i < VM_MAX_CONTEXTS; i++) {
    ASSERT_NOT_NULL(script_execute(0, (uint8_t *)script, NULL, 0));
  }
  while (script_runner_update() == RUNNER_BUSY) {
  }

  for (int i = 0; i < VM_MAX_CONTEXTS; i++) {
    ASSERT_NOT_NULL(script_execute(0, (uint8_t *)script, NULL, 0));
  }
}

// ---------------------------------------------------------------------------
// Opcode dispatch
// ---------------------------------------------------------------------------

TEST(end_opcode_terminates_the_script_and_reports_done) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_END};
  script_execute(0, (uint8_t *)script, NULL, 0);

  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(script_runner_update(), RUNNER_IDLE);
}

TEST(load_scene_opcode_dispatches_to_the_engine_with_its_argument) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_LOAD_SCENE, 7, VM_OP_END};
  script_execute(0, (uint8_t *)script, NULL, 0);
  script_runner_update();

  ASSERT_EQ(stub_scene_load_calls, 1);
  ASSERT_EQ(stub_last_scene_index, 7);
}

TEST(set_scene_tone_opcode_dispatches_to_the_engine_with_its_argument) {
  reset_vm();

  static const uint8_t script[] = {VM_OP_SET_SCENE_TONE, 2, VM_OP_END};
  script_execute(0, (uint8_t *)script, NULL, 0);
  script_runner_update();

  ASSERT_EQ(stub_scene_tone_calls, 1);
  ASSERT_EQ(stub_last_scene_tone, 2);
}

TEST(multiple_opcodes_run_in_sequence_within_a_single_script) {
  reset_vm();

  static const uint8_t script[] = {
      VM_OP_SET_SCENE_TONE, 1, VM_OP_LOAD_SCENE, 3, VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  script_runner_update();

  ASSERT_EQ(stub_scene_tone_calls, 1);
  ASSERT_EQ(stub_last_scene_tone, 1);
  ASSERT_EQ(stub_scene_load_calls, 1);
  ASSERT_EQ(stub_last_scene_index, 3);
}

TEST(unknown_opcode_raises_an_exception_and_terminates_the_script) {
  reset_vm();

  static const uint8_t script[] = {0xEE};
  script_execute(0, (uint8_t *)script, NULL, 0);

  ASSERT_EQ(script_runner_update(), RUNNER_EXCEPTION);
  ASSERT_EQ(vm_exception_code, 0xEE);
  // The offending script should have been torn down — runner is idle again.
  ASSERT_EQ(script_runner_update(), RUNNER_IDLE);
}

// ---------------------------------------------------------------------------
// VM_OP_WAIT
// ---------------------------------------------------------------------------

TEST(wait_opcode_pauses_the_script_for_n_frames) {
  reset_vm();

  static const uint8_t script[] = {
      VM_OP_WAIT, 1, VM_OP_SET_SCENE_TONE, 9, VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);

  // Update 1: WAIT instruction is read — arms a 1-frame countdown. No frame
  // has been "consumed" by the wait yet, nothing past WAIT has run.
  ASSERT_EQ(script_runner_update(), RUNNER_BUSY);
  ASSERT_EQ(stub_scene_tone_calls, 0);

  // Update 2: the countdown ticks down to zero. Still nothing dispatched —
  // this update is entirely consumed by the wait.
  ASSERT_EQ(script_runner_update(), RUNNER_BUSY);
  ASSERT_EQ(stub_scene_tone_calls, 0);

  // Update 3: wait_frames is now 0, so the script resumes and runs to
  // completion in the same update.
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(stub_scene_tone_calls, 1);
  ASSERT_EQ(stub_last_scene_tone, 9);
}

// ---------------------------------------------------------------------------
// Variables & math opcodes
// ---------------------------------------------------------------------------

TEST(set_const_opcode_assigns_an_immediate_value_to_a_variable) {
  reset_vm();

  static const uint8_t script[] = {
      VM_OP_SET_CONST, 0, 42, VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[0], 42);
}

TEST(copy_var_opcode_copies_one_variables_value_into_another) {
  reset_vm();

  static const uint8_t script[] = {
      VM_OP_SET_CONST, 1, 7, VM_OP_COPY_VAR, 2, 1, VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[1], 7);
  ASSERT_EQ(vm_variables[2], 7);
}

TEST(add_const_and_sub_const_opcodes_adjust_a_variable_in_place) {
  reset_vm();

  static const uint8_t script[] = {
      VM_OP_SET_CONST, 0, 10, VM_OP_ADD_CONST, 0, 5,
      VM_OP_SUB_CONST, 0, 3,  VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[0], 12);
}

TEST(add_var_and_sub_var_opcodes_combine_two_variables) {
  reset_vm();

  static const uint8_t script[] = {
      VM_OP_SET_CONST, 0, 10, VM_OP_SET_CONST, 1, 4,
      VM_OP_ADD_VAR,   0, 1,  // var0 = 14
      VM_OP_SET_CONST, 2, 20, VM_OP_SUB_VAR, 2, 1,  // var2 = 16
      VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[0], 14);
  ASSERT_EQ(vm_variables[2], 16);
}

TEST(random_opcode_assigns_a_value_within_the_requested_range) {
  reset_vm();
  vm_seed_random(12345);

  static const uint8_t script[] = {
      VM_OP_RANDOM, 0, 10, 20, VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_TRUE(vm_variables[0] >= 10 && vm_variables[0] <= 20);
}

// ---------------------------------------------------------------------------
// Control flow — jumps & conditional branches
// ---------------------------------------------------------------------------

TEST(jump_opcode_skips_to_a_relative_offset) {
  reset_vm();

  // Layout (byte indices):
  //   0: VM_OP_JUMP
  //   1-2: offset (relative to index 3)
  //   3: VM_OP_SET_CONST  4: var  5: value   <- skipped
  //   6: VM_OP_SET_CONST  7: var  8: value   <- landed on
  //   9: VM_OP_END
  // We want to land on index 6, so offset = 6 - 3 = 3.
  static const uint8_t script[] = {
      VM_OP_JUMP,      3, 0,
      VM_OP_SET_CONST, 0, 111,
      VM_OP_SET_CONST, 0, 222,
      VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[0], 222);
}

TEST(if_var_eq_const_branches_when_the_condition_holds) {
  reset_vm();

  // Layout:
  //   0: VM_OP_SET_CONST 1: var 2: value(5)
  //   3: VM_OP_IF_VAR_EQ_CONST  4: var  5: value(5)  6-7: offset
  //   8: VM_OP_SET_CONST 9: var 10: value(1)         <- skipped on branch
  //   11: VM_OP_SET_CONST 12: var 13: value(2)       <- landed on
  //   14: VM_OP_END
  // offset is relative to index 8 (right after reading the offset operand);
  // landing on index 11 means offset = 11 - 8 = 3.
  static const uint8_t script[] = {
      VM_OP_SET_CONST,       0, 5,
      VM_OP_IF_VAR_EQ_CONST, 0, 5, 3, 0,
      VM_OP_SET_CONST,       1, 1,
      VM_OP_SET_CONST,       1, 2,
      VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[1], 2);
}

TEST(if_var_eq_const_falls_through_when_the_condition_fails) {
  reset_vm();

  // Same layout as above but var0 is 9, so the comparison against 5 is
  // false — execution should fall through and run BOTH SET_CONSTs in turn,
  // leaving var1 with the final value (2).
  static const uint8_t script[] = {
      VM_OP_SET_CONST,       0, 9,
      VM_OP_IF_VAR_EQ_CONST, 0, 5, 3, 0,
      VM_OP_SET_CONST,       1, 1,
      VM_OP_SET_CONST,       1, 2,
      VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[1], 2);
}

TEST(if_var_gt_const_branches_only_when_the_variable_is_greater) {
  reset_vm();

  // var0 = 10, comparing > 5 → true → branch to index 11 (offset 3 from 8).
  static const uint8_t script[] = {
      VM_OP_SET_CONST,       0, 10,
      VM_OP_IF_VAR_GT_CONST, 0, 5, 3, 0,
      VM_OP_SET_CONST,       1, 1,
      VM_OP_SET_CONST,       1, 2,
      VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[1], 2);
}

TEST(jump_and_conditional_branch_implement_a_counting_loop) {
  reset_vm();

  // A classic "repeat until" loop compiled down to raw opcodes:
  //   var0 = 0
  //   loop: var0 += 1
  //         if var0 < 3, jump back to loop
  //
  // Byte layout:
  //   0: VM_OP_SET_CONST  1: var(0)  2: value(0)
  //   3: VM_OP_ADD_CONST  4: var(0)  5: value(1)        <- loop start
  //   6: VM_OP_IF_VAR_LT_CONST 7: var(0) 8: value(3) 9-10: offset
  //   11: VM_OP_END
  //
  // The offset is relative to the PC *after* the 2-byte offset operand is
  // read, i.e. index 11; to land back on index 3 the offset is
  // 3 - 11 = -8 = 0xFFF8 (LE: lo=0xF8, hi=0xFF).
  static const uint8_t script[] = {
      VM_OP_SET_CONST,       0, 0,
      VM_OP_ADD_CONST,       0, 1,
      VM_OP_IF_VAR_LT_CONST, 0, 3, 0xF8, 0xFF,
      VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(vm_variables[0], 3);
}

// ---------------------------------------------------------------------------
// Concurrency — multiple scripts running side by side
// ---------------------------------------------------------------------------

TEST(multiple_scripts_run_concurrently_without_interfering) {
  reset_vm();

  static const uint8_t script_a[] = {VM_OP_SET_SCENE_TONE, 1, VM_OP_END};
  static const uint8_t script_b[] = {VM_OP_SET_SCENE_TONE, 2, VM_OP_END};

  script_execute(0, (uint8_t *)script_a, NULL, 0);
  script_execute(0, (uint8_t *)script_b, NULL, 0);

  // Both scripts set the tone — the important thing is both ran and the
  // runner correctly drains to idle once they're both finished.
  while (script_runner_update() == RUNNER_BUSY) {
  }

  ASSERT_EQ(stub_scene_tone_calls, 2);
  ASSERT_EQ(script_runner_update(), RUNNER_IDLE);
}

TEST(terminate_removes_a_specific_context_without_affecting_others) {
  reset_vm();

  static const uint8_t long_script[] = {VM_OP_WAIT, 250, VM_OP_END};
  static const uint8_t short_script[] = {VM_OP_SET_SCENE_TONE, 5, VM_OP_END};

  uint16_t handle_a = 0;
  SCRIPT_CTX *ctx_a =
      script_execute(0, (uint8_t *)long_script, &handle_a, 0);
  script_execute(0, (uint8_t *)short_script, NULL, 0);

  ASSERT_EQ(script_terminate(ctx_a->ID), 0);
  ASSERT_EQ(handle_a, SCRIPT_TERMINATED);

  // The remaining script should still complete normally.
  while (script_runner_update() == RUNNER_BUSY) {
  }
  ASSERT_EQ(stub_scene_tone_calls, 1);
}

TEST(terminate_reports_failure_for_an_unknown_id) {
  reset_vm();
  ASSERT_EQ(script_terminate(123), 1);
}

// ---------------------------------------------------------------------------
// Camera follow/clamp math (src/camera.c)
//
// A 240x160 viewport (the GBA screen) is used throughout, matching
// SCREEN_WIDTH/SCREEN_HEIGHT — the values the engine actually passes in.
// ---------------------------------------------------------------------------

#define TEST_VIEWPORT_W 240
#define TEST_VIEWPORT_H 160

TEST(camera_centres_on_target_within_a_large_scene) {
  camera_t camera = {0, 0};

  // A scene several screens wide/tall; the target sits well away from every
  // edge, so the camera should simply centre on it: x - viewport/2.
  camera_follow(&camera, 400, 300, TEST_VIEWPORT_W, TEST_VIEWPORT_H, 960, 640);

  ASSERT_EQ(camera.x, 400 - TEST_VIEWPORT_W / 2);
  ASSERT_EQ(camera.y, 300 - TEST_VIEWPORT_H / 2);
}

TEST(camera_clamps_to_zero_at_the_top_left_edge) {
  camera_t camera = {99, 99};

  // Target near the world's origin — centring would scroll into negative
  // territory, which should be clamped to 0 on both axes.
  camera_follow(&camera, 10, 5, TEST_VIEWPORT_W, TEST_VIEWPORT_H, 960, 640);

  ASSERT_EQ(camera.x, 0);
  ASSERT_EQ(camera.y, 0);
}

TEST(camera_clamps_to_max_scroll_at_the_bottom_right_edge) {
  camera_t camera = {0, 0};

  // Target near the world's far corner — centring would scroll past the
  // bottom-right edge, which should be clamped to (world - viewport).
  uint16_t scene_w = 960;
  uint16_t scene_h = 640;
  camera_follow(&camera, scene_w - 5, scene_h - 5, TEST_VIEWPORT_W,
                TEST_VIEWPORT_H, scene_w, scene_h);

  ASSERT_EQ(camera.x, scene_w - TEST_VIEWPORT_W);
  ASSERT_EQ(camera.y, scene_h - TEST_VIEWPORT_H);
}

TEST(camera_locks_to_origin_when_the_scene_fits_within_the_viewport) {
  camera_t camera = {42, 17};

  // A scene no larger than the screen never scrolls — regardless of where
  // the target is, the camera should sit at the origin.
  camera_follow(&camera, 100, 80, TEST_VIEWPORT_W, TEST_VIEWPORT_H, 200, 150);

  ASSERT_EQ(camera.x, 0);
  ASSERT_EQ(camera.y, 0);
}

TEST(camera_locks_a_single_axis_when_only_that_axis_fits) {
  camera_t camera = {0, 0};

  // Wider-than-screen but shorter-than-screen world: X should follow/clamp
  // normally, Y should be locked to 0.
  uint16_t scene_w = 960;
  uint16_t scene_h = 120;
  camera_follow(&camera, 500, 60, TEST_VIEWPORT_W, TEST_VIEWPORT_H, scene_w,
                scene_h);

  ASSERT_EQ(camera.x, 500 - TEST_VIEWPORT_W / 2);
  ASSERT_EQ(camera.y, 0);
}

TEST(camera_follow_is_a_no_op_when_given_a_null_camera) {
  // Should not crash — just defensively does nothing.
  camera_follow(NULL, 100, 100, TEST_VIEWPORT_W, TEST_VIEWPORT_H, 960, 640);
}

// ---------------------------------------------------------------------------
// Tile-based collision (src/collision.c)
//
// All maps below are 4x4 tiles (32x32px, TILE_W/H = 8px) — small enough to
// lay out and reason about by hand, big enough to exercise multi-tile
// rectangles and edge cases.
// ---------------------------------------------------------------------------

TEST(collision_rect_in_open_area_does_not_overlap_solid) {
  // A single solid tile at (2, 1).
  static const uint8_t map[16] = {
      0, 0, 0, 0, //
      0, 0, 1, 0, //
      0, 0, 0, 0, //
      0, 0, 0, 0,
  };

  // Rect entirely within tile (1, 1) — open ground.
  ASSERT_FALSE(collision_rect_overlaps_solid(map, 4, 4, 8, 8, 8, 8));
}

TEST(collision_rect_overlapping_a_solid_tile_collides) {
  static const uint8_t map[16] = {
      0, 0, 0, 0, //
      0, 0, 1, 0, //
      0, 0, 0, 0, //
      0, 0, 0, 0,
  };

  // Rect exactly covering the solid tile at (2, 1) -> px (16, 8).
  ASSERT_TRUE(collision_rect_overlaps_solid(map, 4, 4, 16, 8, 8, 8));
}

TEST(collision_rect_spanning_multiple_tiles_collides_if_any_is_solid) {
  static const uint8_t map[16] = {
      0, 0, 0, 0, //
      0, 0, 1, 0, //
      0, 0, 0, 0, //
      0, 0, 0, 0,
  };

  // A 16x16 rect straddling tiles (1,0)-(2,1): touches the solid (2,1).
  ASSERT_TRUE(collision_rect_overlaps_solid(map, 4, 4, 12, 4, 16, 16));
}

TEST(collision_rect_outside_the_map_bounds_collides) {
  static const uint8_t map[16] = {0};

  // Straddles the left/top edge of the map — out-of-bounds tiles count as
  // solid so actors can't walk off the world.
  ASSERT_TRUE(collision_rect_overlaps_solid(map, 4, 4, -4, -4, 8, 8));
}

TEST(collision_with_a_null_map_never_collides) {
  ASSERT_FALSE(collision_rect_overlaps_solid(NULL, 4, 4, 16, 8, 8, 8));
}

TEST(collision_resolve_movement_is_unobstructed_in_open_area) {
  static const uint8_t map[16] = {0};

  int16_t dx = 0, dy = 0;
  collision_resolve_movement(map, 4, 4, /*left=*/0, /*top=*/0, /*w=*/8,
                             /*h=*/8, /*dx=*/3, /*dy=*/2, &dx, &dy);

  ASSERT_EQ(dx, 3);
  ASSERT_EQ(dy, 2);
}

TEST(collision_resolve_movement_stops_an_actor_at_a_wall) {
  // Solid column at tile x=2, every row — a wall running top to bottom.
  static const uint8_t map[16] = {
      0, 0, 1, 0, //
      0, 0, 1, 0, //
      0, 0, 1, 0, //
      0, 0, 1, 0,
  };

  // Actor at (0, 0), 8x8 — one tile clear of the wall. Asking to move 10px
  // right should stop it flush against the wall (left ends at 8, i.e. its
  // right edge sits at px 15, immediately left of the solid tile at px 16).
  int16_t dx = 99, dy = 99;
  collision_resolve_movement(map, 4, 4, /*left=*/0, /*top=*/0, /*w=*/8,
                             /*h=*/8, /*dx=*/10, /*dy=*/0, &dx, &dy);

  ASSERT_EQ(dx, 8);
  ASSERT_EQ(dy, 0);
}

TEST(collision_resolve_movement_slides_along_a_wall) {
  // Same vertical wall as above (solid column at tile x=2).
  static const uint8_t map[16] = {
      0, 0, 1, 0, //
      0, 0, 1, 0, //
      0, 0, 1, 0, //
      0, 0, 1, 0,
  };

  // Actor at (0, 0), 8x8, asked to move diagonally down-right (10, 10).
  // X is blocked by the wall after 8px (as above); Y runs through open
  // ground at column x=0..1 and should be completely unobstructed — the
  // classic "slide along the wall instead of stopping dead" behaviour.
  int16_t dx = 0, dy = 0;
  collision_resolve_movement(map, 4, 4, /*left=*/0, /*top=*/0, /*w=*/8,
                             /*h=*/8, /*dx=*/10, /*dy=*/10, &dx, &dy);

  ASSERT_EQ(dx, 8);
  ASSERT_EQ(dy, 10);
}

TEST(collision_resolve_movement_handles_null_outputs_gracefully) {
  static const uint8_t map[16] = {0};
  int16_t dx = 0;

  // Should not crash when one of the output pointers is NULL.
  collision_resolve_movement(map, 4, 4, 0, 0, 8, 8, 5, 5, &dx, NULL);
  collision_resolve_movement(map, 4, 4, 0, 0, 8, 8, 5, 5, NULL, &dx);
}

// ---------------------------------------------------------------------------
// Dialogue text helpers (src/text.c)
// ---------------------------------------------------------------------------

TEST(text_format_variables_substitutes_placeholders_with_decimal_values) {
  reset_vm();
  vm_variables[5] = 42;
  vm_variables[1] = 7;

  char out[64];
  size_t written =
      text_format_variables("You have {5} gold and {1} potions", out,
                            sizeof(out));

  ASSERT_EQ(strcmp(out, "You have 42 gold and 7 potions"), 0);
  ASSERT_EQ(written, strlen(out));
}

TEST(text_format_variables_renders_negative_values_with_a_minus_sign) {
  reset_vm();
  vm_variables[1] = -7;

  char out[32];
  text_format_variables("HP: {1}", out, sizeof(out));

  ASSERT_EQ(strcmp(out, "HP: -7"), 0);
}

TEST(text_format_variables_leaves_malformed_placeholders_untouched) {
  reset_vm();

  char out[64];
  static const char *input = "{abc} and {12 and {";
  text_format_variables(input, out, sizeof(out));

  ASSERT_EQ(strcmp(out, input), 0);
}

TEST(text_format_variables_treats_out_of_range_indices_as_literal_text) {
  reset_vm();

  char out[16];
  text_format_variables("{999}", out, sizeof(out));

  ASSERT_EQ(strcmp(out, "{999}"), 0);
}

TEST(text_format_variables_truncates_to_fit_the_output_buffer) {
  reset_vm();
  vm_variables[0] = 12345;

  char out[6];
  size_t written = text_format_variables("{0}-tail", out, sizeof(out));

  // Buffer holds 5 chars + NUL; "12345" alone already fills it.
  ASSERT_EQ(written, 5u);
  ASSERT_EQ(strlen(out), 5u);
  ASSERT_EQ(strcmp(out, "12345"), 0);
}

TEST(text_word_wrap_leaves_short_text_on_a_single_line) {
  char out[32];
  text_word_wrap("Hi there", 20, out, sizeof(out));

  ASSERT_EQ(strcmp(out, "Hi there"), 0);
}

TEST(text_word_wrap_breaks_at_word_boundaries_when_a_line_overflows) {
  char out[32];
  text_word_wrap("one two three", 7, out, sizeof(out));

  ASSERT_EQ(strcmp(out, "one two\nthree"), 0);
}

TEST(text_word_wrap_collapses_whitespace_runs_into_single_spaces) {
  char out[32];
  text_word_wrap("a    b\tc", 20, out, sizeof(out));

  ASSERT_EQ(strcmp(out, "a b c"), 0);
}

TEST(text_word_wrap_preserves_explicit_newlines_alongside_wrap_breaks) {
  char out[32];
  // "ab" fits; "cd ef" fits on the second (explicit) line; "gh" doesn't fit
  // alongside "cd ef" and must wrap onto a third line of its own.
  text_word_wrap("ab\ncd ef gh", 5, out, sizeof(out));

  ASSERT_EQ(strcmp(out, "ab\ncd ef\ngh"), 0);
}

TEST(text_word_wrap_hard_breaks_a_single_word_longer_than_the_line) {
  char out[32];
  text_word_wrap("abcdefgh", 3, out, sizeof(out));

  ASSERT_EQ(strcmp(out, "abc\ndef\ngh"), 0);
}

TEST(text_word_wrap_only_applies_hard_breaks_when_wrapping_is_disabled) {
  char out[32];
  text_word_wrap("a b\nc", 0, out, sizeof(out));

  ASSERT_EQ(strcmp(out, "a b\nc"), 0);
}

TEST(text_word_wrap_truncates_to_fit_the_output_buffer) {
  char out[4];
  size_t written = text_word_wrap("abcdefgh", 0, out, sizeof(out));

  ASSERT_EQ(written, 3u);
  ASSERT_EQ(strlen(out), 3u);
  ASSERT_EQ(strcmp(out, "abc"), 0);
}

// ---------------------------------------------------------------------------
// Save data encode/decode (src/savegame.c)
// ---------------------------------------------------------------------------

static save_state_t make_sample_save_state(void) {
  save_state_t state;
  state.scene_index = 3;
  state.player_x = 120;
  state.player_y = 88;
  for (size_t i = 0; i < VM_VARIABLE_COUNT; i++) {
    // A mix of positive, negative, and zero values across the range.
    state.variables[i] = (int16_t)((int)i * 7 - 500);
  }
  return state;
}

TEST(save_encode_reports_the_full_record_size_on_success) {
  save_state_t state = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];

  ASSERT_EQ(save_encode(&state, buffer, sizeof(buffer)), SAVE_RECORD_SIZE);
}

TEST(save_encode_refuses_a_buffer_smaller_than_the_record) {
  save_state_t state = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];

  ASSERT_EQ(save_encode(&state, buffer, SAVE_RECORD_SIZE - 1), 0u);
}

TEST(save_round_trip_preserves_every_field) {
  save_state_t original = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];
  ASSERT_EQ(save_encode(&original, buffer, sizeof(buffer)), SAVE_RECORD_SIZE);

  save_state_t restored;
  memset(&restored, 0, sizeof(restored));
  ASSERT_TRUE(save_decode(buffer, sizeof(buffer), &restored));

  ASSERT_EQ(restored.scene_index, original.scene_index);
  ASSERT_EQ(restored.player_x, original.player_x);
  ASSERT_EQ(restored.player_y, original.player_y);
  for (size_t i = 0; i < VM_VARIABLE_COUNT; i++) {
    ASSERT_EQ(restored.variables[i], original.variables[i]);
  }
}

TEST(save_decode_rejects_a_buffer_smaller_than_the_record) {
  save_state_t original = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];
  save_encode(&original, buffer, sizeof(buffer));

  save_state_t restored;
  ASSERT_FALSE(save_decode(buffer, SAVE_RECORD_SIZE - 1, &restored));
}

TEST(save_decode_rejects_data_with_a_bad_magic_number) {
  save_state_t original = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];
  save_encode(&original, buffer, sizeof(buffer));

  // Corrupt the first magic byte — simulates uninitialised/foreign SRAM.
  buffer[0] ^= 0xFF;

  save_state_t restored;
  ASSERT_FALSE(save_decode(buffer, sizeof(buffer), &restored));
}

TEST(save_decode_rejects_data_from_an_incompatible_format_version) {
  save_state_t original = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];
  save_encode(&original, buffer, sizeof(buffer));

  buffer[4] += 1; // version byte

  save_state_t restored;
  ASSERT_FALSE(save_decode(buffer, sizeof(buffer), &restored));
}

TEST(save_decode_rejects_data_with_a_corrupted_checksum) {
  save_state_t original = make_sample_save_state();
  static uint8_t buffer[SAVE_RECORD_SIZE];
  save_encode(&original, buffer, sizeof(buffer));

  // Flip a byte well inside the variable payload — magic/version still
  // check out, but the checksum no longer matches.
  buffer[SAVE_RECORD_SIZE / 2] ^= 0xFF;

  save_state_t restored;
  ASSERT_FALSE(save_decode(buffer, sizeof(buffer), &restored));
}

TEST(save_decode_leaves_the_output_untouched_on_failure) {
  static uint8_t blank[SAVE_RECORD_SIZE] = {0}; // never a valid record

  save_state_t restored;
  memset(&restored, 0xAB, sizeof(restored));
  ASSERT_FALSE(save_decode(blank, sizeof(blank), &restored));

  // Sentinel bytes should be exactly as we left them.
  uint8_t *raw = (uint8_t *)&restored;
  bool untouched = true;
  for (size_t i = 0; i < sizeof(restored); i++) {
    if (raw[i] != 0xAB) {
      untouched = false;
      break;
    }
  }
  ASSERT_TRUE(untouched);
}

// ---------------------------------------------------------------------------
// Movement types (movement.c)
// ---------------------------------------------------------------------------

TEST(movement_patrol_paces_horizontally_across_a_wide_bounds) {
  bool moving_positive = true;
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  // Bounds wider than tall (16x8) -> horizontal patrol. Actor mid-way,
  // already heading toward the max edge.
  movement_patrol(20, 10, 16, 10, 16, 8, 1, &moving_positive, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 1);
  ASSERT_EQ(vel_y, 0);
  ASSERT_TRUE(moving_positive);
}

TEST(movement_patrol_reverses_at_the_positive_edge) {
  bool moving_positive = true;
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  // Actor has reached (or passed) the right edge of a 16px-wide bounds
  // starting at x=16 -> max edge is x=32.
  movement_patrol(32, 10, 16, 10, 16, 8, 2, &moving_positive, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, -2);
  ASSERT_EQ(vel_y, 0);
  ASSERT_FALSE(moving_positive);
}

TEST(movement_patrol_reverses_at_the_negative_edge) {
  bool moving_positive = false;
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  // Actor has reached (or passed) the left edge of the bounds (x=16).
  movement_patrol(14, 10, 16, 10, 16, 8, 2, &moving_positive, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 2);
  ASSERT_EQ(vel_y, 0);
  ASSERT_TRUE(moving_positive);
}

TEST(movement_patrol_paces_vertically_across_a_tall_bounds) {
  bool moving_positive = false;
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  // Bounds taller than wide (8x16) -> vertical patrol. Actor mid-way,
  // currently heading toward the min (top) edge.
  movement_patrol(10, 20, 10, 16, 8, 16, 1, &moving_positive, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 0);
  ASSERT_EQ(vel_y, -1);
  ASSERT_FALSE(moving_positive);
}

TEST(movement_patrol_stays_still_with_a_degenerate_bounds) {
  bool moving_positive = true;
  int16_t vel_x = 7;
  int16_t vel_y = 7;

  movement_patrol(10, 10, 10, 10, 0, 0, 3, &moving_positive, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 0);
  ASSERT_EQ(vel_y, 0);
}

TEST(movement_follow_closes_in_on_a_target_within_range) {
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  // Target is 10px right and 3px down; speed 2 clamps each axis.
  movement_follow(0, 0, 10, 3, 2, 32, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 2);
  ASSERT_EQ(vel_y, 2);
}

TEST(movement_follow_does_not_overshoot_on_the_final_approach) {
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  // Only 1px of horizontal distance remains; speed 2 should not overshoot.
  movement_follow(9, 5, 10, 5, 2, 32, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 1);
  ASSERT_EQ(vel_y, 0);
}

TEST(movement_follow_stays_put_when_the_target_is_out_of_range) {
  int16_t vel_x = 9;
  int16_t vel_y = 9;

  // Target is 40px away on the X axis; range is only 32px.
  movement_follow(0, 0, 40, 0, 2, 32, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, 0);
  ASSERT_EQ(vel_y, 0);
}

TEST(movement_follow_moves_toward_a_target_that_is_behind_the_actor) {
  int16_t vel_x = 0;
  int16_t vel_y = 0;

  movement_follow(20, 20, 10, 25, 3, 32, &vel_x, &vel_y);

  ASSERT_EQ(vel_x, -3);
  ASSERT_EQ(vel_y, 3);
}

// ---------------------------------------------------------------------------
// Scene triggers (trigger.c)
// ---------------------------------------------------------------------------

TEST(trigger_rects_overlap_detects_intersecting_boxes) {
  ASSERT_TRUE(trigger_rects_overlap(0, 0, 16, 16, 8, 8, 16, 16));
}

TEST(trigger_rects_overlap_is_false_for_separated_boxes) {
  ASSERT_FALSE(trigger_rects_overlap(0, 0, 16, 16, 32, 32, 16, 16));
}

TEST(trigger_rects_overlap_treats_touching_edges_as_not_overlapping) {
  // Boxes that merely share an edge (no interior overlap) should not count
  // — an actor standing just outside a doorway hasn't stepped into it yet.
  ASSERT_FALSE(trigger_rects_overlap(0, 0, 16, 16, 16, 0, 16, 16));
}

TEST(trigger_rects_overlap_is_false_for_zero_area_rects) {
  ASSERT_FALSE(trigger_rects_overlap(0, 0, 0, 16, 0, 0, 16, 16));
  ASSERT_FALSE(trigger_rects_overlap(0, 0, 16, 16, 0, 0, 0, 16));
}

TEST(trigger_rects_overlap_handles_negative_coordinates) {
  ASSERT_TRUE(trigger_rects_overlap(-8, -8, 16, 16, 0, 0, 16, 16));
  ASSERT_FALSE(trigger_rects_overlap(-32, -32, 16, 16, 0, 0, 16, 16));
}

TEST(trigger_find_overlap_returns_the_first_matching_zone) {
  static const int16_t zone_x[] = {0, 100, 100};
  static const int16_t zone_y[] = {0, 100, 100};
  static const uint16_t zone_w[] = {16, 16, 16};
  static const uint16_t zone_h[] = {16, 16, 16};

  // Actor overlaps zones 1 and 2 (both at 100,100) — must report the lower
  // index so compiled trigger order is honoured deterministically.
  int found = trigger_find_overlap(104, 104, 8, 8, zone_x, zone_y, zone_w,
                                   zone_h, 3);

  ASSERT_EQ(found, 1);
}

TEST(trigger_find_overlap_returns_none_when_nothing_overlaps) {
  static const int16_t zone_x[] = {100};
  static const int16_t zone_y[] = {100};
  static const uint16_t zone_w[] = {16};
  static const uint16_t zone_h[] = {16};

  int found =
      trigger_find_overlap(0, 0, 8, 8, zone_x, zone_y, zone_w, zone_h, 1);

  ASSERT_EQ(found, TRIGGER_NONE);
}

TEST(trigger_find_overlap_handles_a_null_zone_list_safely) {
  int found = trigger_find_overlap(0, 0, 8, 8, NULL, NULL, NULL, NULL, 5);

  ASSERT_EQ(found, TRIGGER_NONE);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) {
  RUN_TEST(init_starts_with_no_running_contexts);
  RUN_TEST(init_clears_exception_state);

  RUN_TEST(execute_returns_a_context_and_assigns_a_handle);
  RUN_TEST(execute_pushes_arguments_onto_the_context_stack);
  RUN_TEST(execute_runs_out_of_contexts_gracefully);
  RUN_TEST(terminated_contexts_are_returned_to_the_free_list);

  RUN_TEST(end_opcode_terminates_the_script_and_reports_done);
  RUN_TEST(load_scene_opcode_dispatches_to_the_engine_with_its_argument);
  RUN_TEST(set_scene_tone_opcode_dispatches_to_the_engine_with_its_argument);
  RUN_TEST(multiple_opcodes_run_in_sequence_within_a_single_script);
  RUN_TEST(unknown_opcode_raises_an_exception_and_terminates_the_script);

  RUN_TEST(wait_opcode_pauses_the_script_for_n_frames);

  RUN_TEST(set_const_opcode_assigns_an_immediate_value_to_a_variable);
  RUN_TEST(copy_var_opcode_copies_one_variables_value_into_another);
  RUN_TEST(add_const_and_sub_const_opcodes_adjust_a_variable_in_place);
  RUN_TEST(add_var_and_sub_var_opcodes_combine_two_variables);
  RUN_TEST(random_opcode_assigns_a_value_within_the_requested_range);

  RUN_TEST(jump_opcode_skips_to_a_relative_offset);
  RUN_TEST(if_var_eq_const_branches_when_the_condition_holds);
  RUN_TEST(if_var_eq_const_falls_through_when_the_condition_fails);
  RUN_TEST(if_var_gt_const_branches_only_when_the_variable_is_greater);
  RUN_TEST(jump_and_conditional_branch_implement_a_counting_loop);

  RUN_TEST(camera_centres_on_target_within_a_large_scene);
  RUN_TEST(camera_clamps_to_zero_at_the_top_left_edge);
  RUN_TEST(camera_clamps_to_max_scroll_at_the_bottom_right_edge);
  RUN_TEST(camera_locks_to_origin_when_the_scene_fits_within_the_viewport);
  RUN_TEST(camera_locks_a_single_axis_when_only_that_axis_fits);
  RUN_TEST(camera_follow_is_a_no_op_when_given_a_null_camera);

  RUN_TEST(collision_rect_in_open_area_does_not_overlap_solid);
  RUN_TEST(collision_rect_overlapping_a_solid_tile_collides);
  RUN_TEST(collision_rect_spanning_multiple_tiles_collides_if_any_is_solid);
  RUN_TEST(collision_rect_outside_the_map_bounds_collides);
  RUN_TEST(collision_with_a_null_map_never_collides);
  RUN_TEST(collision_resolve_movement_is_unobstructed_in_open_area);
  RUN_TEST(collision_resolve_movement_stops_an_actor_at_a_wall);
  RUN_TEST(collision_resolve_movement_slides_along_a_wall);
  RUN_TEST(collision_resolve_movement_handles_null_outputs_gracefully);

  RUN_TEST(text_format_variables_substitutes_placeholders_with_decimal_values);
  RUN_TEST(text_format_variables_renders_negative_values_with_a_minus_sign);
  RUN_TEST(text_format_variables_leaves_malformed_placeholders_untouched);
  RUN_TEST(text_format_variables_treats_out_of_range_indices_as_literal_text);
  RUN_TEST(text_format_variables_truncates_to_fit_the_output_buffer);
  RUN_TEST(text_word_wrap_leaves_short_text_on_a_single_line);
  RUN_TEST(text_word_wrap_breaks_at_word_boundaries_when_a_line_overflows);
  RUN_TEST(text_word_wrap_collapses_whitespace_runs_into_single_spaces);
  RUN_TEST(text_word_wrap_preserves_explicit_newlines_alongside_wrap_breaks);
  RUN_TEST(text_word_wrap_hard_breaks_a_single_word_longer_than_the_line);
  RUN_TEST(text_word_wrap_only_applies_hard_breaks_when_wrapping_is_disabled);
  RUN_TEST(text_word_wrap_truncates_to_fit_the_output_buffer);

  RUN_TEST(save_encode_reports_the_full_record_size_on_success);
  RUN_TEST(save_encode_refuses_a_buffer_smaller_than_the_record);
  RUN_TEST(save_round_trip_preserves_every_field);
  RUN_TEST(save_decode_rejects_a_buffer_smaller_than_the_record);
  RUN_TEST(save_decode_rejects_data_with_a_bad_magic_number);
  RUN_TEST(save_decode_rejects_data_from_an_incompatible_format_version);
  RUN_TEST(save_decode_rejects_data_with_a_corrupted_checksum);
  RUN_TEST(save_decode_leaves_the_output_untouched_on_failure);

  RUN_TEST(movement_patrol_paces_horizontally_across_a_wide_bounds);
  RUN_TEST(movement_patrol_reverses_at_the_positive_edge);
  RUN_TEST(movement_patrol_reverses_at_the_negative_edge);
  RUN_TEST(movement_patrol_paces_vertically_across_a_tall_bounds);
  RUN_TEST(movement_patrol_stays_still_with_a_degenerate_bounds);
  RUN_TEST(movement_follow_closes_in_on_a_target_within_range);
  RUN_TEST(movement_follow_does_not_overshoot_on_the_final_approach);
  RUN_TEST(movement_follow_stays_put_when_the_target_is_out_of_range);
  RUN_TEST(movement_follow_moves_toward_a_target_that_is_behind_the_actor);

  RUN_TEST(trigger_rects_overlap_detects_intersecting_boxes);
  RUN_TEST(trigger_rects_overlap_is_false_for_separated_boxes);
  RUN_TEST(trigger_rects_overlap_treats_touching_edges_as_not_overlapping);
  RUN_TEST(trigger_rects_overlap_is_false_for_zero_area_rects);
  RUN_TEST(trigger_rects_overlap_handles_negative_coordinates);
  RUN_TEST(trigger_find_overlap_returns_the_first_matching_zone);
  RUN_TEST(trigger_find_overlap_returns_none_when_nothing_overlaps);
  RUN_TEST(trigger_find_overlap_handles_a_null_zone_list_safely);

  RUN_TEST(multiple_scripts_run_concurrently_without_interfering);
  RUN_TEST(terminate_removes_a_specific_context_without_affecting_others);
  RUN_TEST(terminate_reports_failure_for_an_unknown_id);

  return TEST_REPORT();
}
