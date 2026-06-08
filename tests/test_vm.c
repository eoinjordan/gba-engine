// Unit tests for the bytecode script runner (src/vm.c).
//
// vm.c is plain, portable C with no hardware dependencies, so it can be
// compiled and exercised directly on the host. These tests cover context
// allocation/recycling, opcode dispatch, waiting, and exception handling —
// the parts of the engine most likely to harbour subtle logic bugs and least
// likely to need a real GBA (or emulator) to validate.

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
      VM_OP_WAIT, 2, VM_OP_SET_SCENE_TONE, 9, VM_OP_END,
  };
  script_execute(0, (uint8_t *)script, NULL, 0);

  // Frame 1: enters the wait, decrements once, nothing dispatched yet.
  ASSERT_EQ(script_runner_update(), RUNNER_BUSY);
  ASSERT_EQ(stub_scene_tone_calls, 0);

  // Frame 2: still waiting.
  ASSERT_EQ(script_runner_update(), RUNNER_BUSY);
  ASSERT_EQ(stub_scene_tone_calls, 0);

  // Frame 3: wait elapses, the rest of the script runs to completion.
  ASSERT_EQ(script_runner_update(), RUNNER_DONE);
  ASSERT_EQ(stub_scene_tone_calls, 1);
  ASSERT_EQ(stub_last_scene_tone, 9);
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

  RUN_TEST(multiple_scripts_run_concurrently_without_interfering);
  RUN_TEST(terminate_removes_a_specific_context_without_affecting_others);
  RUN_TEST(terminate_reports_failure_for_an_unknown_id);

  return TEST_REPORT();
}
