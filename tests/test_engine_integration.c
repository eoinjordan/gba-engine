#include "engine.h"
#include "gba_system.h"
#include "test_engine_stubs.h"
#include "test_framework.h"

static uint16_t *screenblock(unsigned block) {
  return &test_mem_vram[(block * 0x0800u) / sizeof(uint16_t)];
}

static void reset_engine(void) {
  test_reset_environment();
  engine_init();
  test_set_keys(0);
}

TEST(engine_init_schedules_bootstrap_and_enables_bg0) {
  reset_engine();

  ASSERT_EQ(test_reg_dispcnt, MODE_0 | BG0_ENABLE);
  ASSERT_EQ(test_reg_bg0cnt, 0);
  ASSERT_EQ(test_load_palette_calls, 0);

  engine_update();

  ASSERT_EQ(test_reg_bg0cnt, (28u << 8));
  ASSERT_EQ(test_load_palette_calls, 1);
}

TEST(load_scene_renders_expected_border_collision_and_checker_tiles) {
  reset_engine();
  load_scene(0);

  uint16_t *map = screenblock(28);
  ASSERT_EQ(map[0], 2);
  ASSERT_EQ(map[1 * 32 + 1], 1);
  ASSERT_EQ(map[2 * 32 + 2], 4);
  ASSERT_EQ(map[1 * 32 + 3], 3);
  ASSERT_EQ(map[8 * 32 + 8], 0);
}

TEST(vm_scene_set_tone_reloads_the_palette_for_the_current_scene) {
  reset_engine();
  load_scene(0);

  uint16_t original = test_mem_palette[0];
  vm_scene_set_tone(1);

  ASSERT_TRUE(original != test_mem_palette[0]);
  ASSERT_EQ(test_mem_palette[0], RGB15(2, 5, 8));
}

TEST(engine_update_cycles_scenes_when_start_is_pressed) {
  reset_engine();
  engine_update();

  test_set_keys(KEY_START);
  engine_update();

  uint16_t *map = screenblock(28);
  ASSERT_EQ(test_mem_palette[0], RGB15(4, 1, 5));
  ASSERT_EQ(map[3 * 32 + 3], 2);
  ASSERT_EQ(map[4 * 32 + 4], 0);
}

TEST(active_actors_move_and_destroyed_slots_are_reused) {
  reset_engine();
  load_scene(0);

  actor_t *first = spawn_actor(7, 10, 12);
  ASSERT_NOT_NULL(first);
  first->vel_x = 2;
  first->vel_y = 3;
  first->anim_speed = 1;

  engine_update();

  ASSERT_EQ(first->x, 12);
  ASSERT_EQ(first->y, 15);
  ASSERT_EQ(first->anim_frame, 1);

  actor_t *second = spawn_actor(9, 20, 24);
  ASSERT_NOT_NULL(second);
  destroy_actor(second);

  actor_t *reused = spawn_actor(11, 30, 32);
  ASSERT_EQ(reused, second);
  ASSERT_EQ(reused->sprite_index, 11);
}

TEST(movement_type_patrol_paces_back_and_forth_across_its_bounds) {
  reset_engine();
  load_scene(0);

  // actors[0] is the camera/player target; keep it stationary and well away
  // from the patrol actor so it can't be mistaken for the follow target.
  actor_t *player = spawn_actor(0, 4, 4);
  ASSERT_NOT_NULL(player);

  // Horizontal patrol (bounds_w >= bounds_h): bounds span pixels [8,24) on
  // the x axis at a fixed y row clear of scene0's single collision tile
  // (tile (2,2), i.e. pixels [16,24)x[16,24) — our patrol stays at y=40).
  actor_t *patroller = spawn_actor(1, 22, 40);
  ASSERT_NOT_NULL(patroller);
  patroller->movement_type = MOVEMENT_TYPE_PATROL;
  patroller->move_speed = 2;
  patroller->movement_bounds_x = 8;
  patroller->movement_bounds_y = 40;
  patroller->movement_bounds_w = 16;
  patroller->movement_bounds_h = 4;

  // Starts heading toward the max edge (24): one step lands exactly on it.
  engine_update();
  ASSERT_EQ(patroller->vel_x, 2);
  ASSERT_EQ(patroller->vel_y, 0);
  ASSERT_EQ(patroller->x, 24);
  ASSERT_TRUE(patroller->movement_positive);

  // Having reached the max edge, the next frame reverses direction.
  engine_update();
  ASSERT_EQ(patroller->vel_x, -2);
  ASSERT_EQ(patroller->x, 22);
  ASSERT_TRUE(!patroller->movement_positive);
}

TEST(movement_type_follow_chases_the_player_only_within_range) {
  reset_engine();
  load_scene(0);

  actor_t *player = spawn_actor(0, 30, 8);
  ASSERT_NOT_NULL(player);

  actor_t *follower = spawn_actor(1, 10, 8);
  ASSERT_NOT_NULL(follower);
  follower->movement_type = MOVEMENT_TYPE_FOLLOW;
  follower->move_speed = 2;
  follower->movement_bounds_w = 30; // square aggro range, in pixels

  // Player is 20px away on the x axis (within the 30px range): follower
  // steps toward it, clamped to its speed.
  engine_update();
  ASSERT_EQ(follower->vel_x, 2);
  ASSERT_EQ(follower->vel_y, 0);
  ASSERT_EQ(follower->x, 12);

  // Move the player out of range (32px away): follower stops dead, even
  // though it last had nonzero velocity.
  player->x = 44;
  engine_update();
  ASSERT_EQ(follower->vel_x, 0);
  ASSERT_EQ(follower->vel_y, 0);
  ASSERT_EQ(follower->x, 12);
}

TEST(scene_trigger_runs_its_script_once_when_the_player_enters) {
  reset_engine();
  load_scene(2);

  // test_scene2's only trigger covers tile (2,2)-(4,4), i.e. pixels
  // [16,32)x[16,32); its script sets the palette tone to 2
  // (RGB15(6, 3, 1) — see bg_palettes — distinct from scene2's initial
  // tone 0, RGB15(1, 3, 4)), an effect already proven observable via
  // test_mem_palette by vm_scene_set_tone_reloads_the_palette_for_the_current_scene.
  actor_t *player = spawn_actor(0, 0, 0);
  ASSERT_NOT_NULL(player);

  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(1, 3, 4));

  // Step the player into the trigger zone. Setting position directly (not
  // via vel_x/collision) keeps this test focused purely on trigger overlap.
  player->x = 20;
  player->y = 20;

  // Frame 1: engine_update detects the new overlap and schedules the
  // trigger's script — but script_runner_update already ran earlier in this
  // same frame, so the palette hasn't changed yet.
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(1, 3, 4));

  // Frame 2: the scheduled script now runs to completion (VM_OP_SET_SCENE_TONE
  // then VM_OP_END), reloading the palette for tone 2.
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(6, 3, 1));
}

int main(void) {
  RUN_TEST(engine_init_schedules_bootstrap_and_enables_bg0);
  RUN_TEST(load_scene_renders_expected_border_collision_and_checker_tiles);
  RUN_TEST(vm_scene_set_tone_reloads_the_palette_for_the_current_scene);
  RUN_TEST(engine_update_cycles_scenes_when_start_is_pressed);
  RUN_TEST(active_actors_move_and_destroyed_slots_are_reused);
  RUN_TEST(movement_type_patrol_paces_back_and_forth_across_its_bounds);
  RUN_TEST(movement_type_follow_chases_the_player_only_within_range);
  RUN_TEST(scene_trigger_runs_its_script_once_when_the_player_enters);
  return TEST_REPORT();
}
