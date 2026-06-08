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
#include "test_framework.h"
#include "test_stubs.h"
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

  RUN_TEST(multiple_scripts_run_concurrently_without_interfering);
  RUN_TEST(terminate_removes_a_specific_context_without_affecting_others);
  RUN_TEST(terminate_reports_failure_for_an_unknown_id);

  return TEST_REPORT();
}
