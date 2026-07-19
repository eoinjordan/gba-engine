#include "engine.h"
#include "gba_system.h"
#include "test_engine_stubs.h"
#include "test_framework.h"
#include "textbox.h"
#include "vm.h"

static uint16_t *screenblock(unsigned block) {
  return &test_mem_vram[(block * 0x0800u) / sizeof(uint16_t)];
}

static uint16_t *charblock(unsigned block) {
  return &test_mem_vram[(block * 0x4000u) / sizeof(uint16_t)];
}

static void reset_engine(void) {
  test_reset_environment();
  engine_init();
  test_set_keys(0);
}

TEST(engine_init_schedules_bootstrap_and_enables_bg0) {
  reset_engine();

  ASSERT_EQ(test_reg_dispcnt, MODE_0 | BG0_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
  ASSERT_EQ(test_reg_bg0cnt, 0);
  ASSERT_EQ(test_load_palette_calls, 0);

  engine_update();

  ASSERT_EQ(test_reg_bg0cnt, (28u << 8) | 1u);
  ASSERT_EQ(test_load_palette_calls, 1);
}

TEST(load_scene_renders_compiled_tilemap_and_tileset) {
  reset_engine();
  load_scene(0);

  uint16_t *map = screenblock(28);
  uint16_t *tiles = charblock(0);
  ASSERT_EQ(tiles[16], 0x1111);
  ASSERT_EQ(tiles[32], 0x2222);
  ASSERT_EQ(tiles[48], 0x3333);
  ASSERT_EQ(map[0], 1);
  ASSERT_EQ(map[1], 2);
  ASSERT_EQ(map[2], 3);
  ASSERT_EQ(map[2 * 32 + 2], 1);
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
  // Scene 1's start script runs in the same update and selects tone 3.
  ASSERT_EQ(test_mem_palette[0], RGB15(4, 1, 5));
  ASSERT_EQ(map[3 * 32 + 3], 1);
  ASSERT_EQ(map[4 * 32 + 4], 0);
}

TEST(textbox_dismiss_input_is_not_reused_for_movement_or_interaction) {
  static uint8_t dialogue_script[] = {
      VM_OP_SHOW_TEXT, 'T', 'a', 'l', 'k', 0,
      VM_OP_END,
  };

  reset_engine();
  engine_update(); // Drain bootstrap.
  load_scene(4);   // Spawn the adjacent interaction test actor.

  uint16_t original_palette = test_mem_palette[0];
  ASSERT_TRUE(vm_actor_at_position(0, 0, 0));

  script_execute(0, dialogue_script, NULL, 0);
  engine_update();
  ASSERT_TRUE(textbox_is_open());

  // A dismisses the text; RIGHT deliberately shares the frame to prove all
  // gameplay input is consumed while dialogue owns the controls.
  test_set_keys(KEY_A | KEY_RIGHT);
  engine_update();
  ASSERT_TRUE(!textbox_is_open());
  ASSERT_TRUE(vm_actor_at_position(0, 0, 0));

  // If the dismissing A press leaked into actor interaction, the adjacent
  // actor's script would set scene tone 2 on this next runner update.
  test_set_keys(0);
  engine_update();
  ASSERT_EQ(test_mem_palette[0], original_palette);
}

TEST(load_scene_schedules_its_start_script_after_spawning_actors) {
  reset_engine();
  engine_update(); // Drain bootstrap for scene 0.

  load_scene(1);
  ASSERT_EQ(test_mem_palette[0], RGB15(1, 3, 4));

  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(4, 1, 5));
}

TEST(active_actors_move_and_destroyed_slots_are_reused) {
  reset_engine();
  // Drain the bootstrap script (LOAD_SCENE 0) that engine_init scheduled but
  // never ran — otherwise it fires on our first engine_update() below
  // (alongside the actor movement we're testing) and reloads scene 0,
  // wiping the actor we're about to spawn. (See
  // engine_update_cycles_scenes_when_start_is_pressed for the same drain.)
  engine_update();
  load_scene(0);

  // Spawned well clear of scene0's single collision tile (tile (2,2), i.e.
  // pixels [16,24)x[16,24)) — at (10,12) with the actor's 8x8 bounds, it
  // would already overlap that wall and collision_resolve_movement would
  // (correctly) refuse to move it at all, which isn't what this test is
  // about. (4,4) -> (6,7) stays clear of the wall on both legs.
  actor_t *first = spawn_actor(7, 4, 4);
  ASSERT_NOT_NULL(first);
  first->vel_x = 2;
  first->vel_y = 3;
  first->anim_speed = 1;

  engine_update();

  ASSERT_EQ(first->x, 6);
  ASSERT_EQ(first->y, 7);
  ASSERT_EQ(first->anim_frame, 1);

  actor_t *second = spawn_actor(9, 20, 24);
  ASSERT_NOT_NULL(second);
  destroy_actor(second);

  actor_t *reused = spawn_actor(11, 30, 32);
  ASSERT_TRUE(reused == second);
  ASSERT_EQ(reused->sprite_index, 11);
}

TEST(movement_type_patrol_paces_back_and_forth_across_its_bounds) {
  reset_engine();
  // Drain the bootstrap script (LOAD_SCENE 0) that engine_init scheduled but
  // never ran — otherwise it fires on our first engine_update() below
  // (alongside the patrol movement we're testing) and reloads scene 0,
  // wiping both actors we're about to spawn. (See
  // engine_update_cycles_scenes_when_start_is_pressed for the same drain.)
  engine_update();
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
  // Drain the bootstrap script (LOAD_SCENE 0) that engine_init scheduled but
  // never ran — otherwise it fires on our first engine_update() below
  // (alongside the follow movement we're testing) and reloads scene 0,
  // wiping both actors we're about to spawn. (See
  // engine_update_cycles_scenes_when_start_is_pressed for the same drain.)
  engine_update();
  load_scene(0);

  actor_t *follower = spawn_actor(1, 10, 0);
  ASSERT_NOT_NULL(follower);
  follower->movement_type = MOVEMENT_TYPE_FOLLOW;
  follower->move_speed = 2;
  follower->movement_bounds_w = 30; // square aggro range, in pixels

  // The auto-spawned player actor is at (0,0), within range: follower steps
  // toward it, clamped to speed.
  engine_update();
  ASSERT_EQ(follower->vel_x, -2);
  ASSERT_EQ(follower->vel_y, 0);
  ASSERT_EQ(follower->x, 8);

  // Move the follower out of range: it stops dead, even though it last had
  // nonzero velocity.
  follower->x = 44;
  engine_update();
  ASSERT_EQ(follower->vel_x, 0);
  ASSERT_EQ(follower->vel_y, 0);
  ASSERT_EQ(follower->x, 44);
}

TEST(scene_trigger_runs_its_script_once_when_the_player_enters) {
  reset_engine();
  // Drain the bootstrap script (LOAD_SCENE 0) that engine_init scheduled but
  // never ran — otherwise it fires on our first engine_update() below
  // (alongside the trigger overlap we're testing), reloading *scene 0*
  // (not the scene 2 we're about to explicitly load) and wiping our player
  // actor and current_scene_def/current_palette_tone out from under us. (See
  // engine_update_cycles_scenes_when_start_is_pressed for the same drain.)
  engine_update();
  load_scene(2);

  // test_scene2's only trigger covers the auto-spawned player at tile (0,0);
  // its script sets the palette tone to 2
  // (RGB15(6, 3, 1) — see bg_palettes — distinct from scene2's initial
  // tone 0, RGB15(1, 3, 4)), an effect already proven observable via
  // test_mem_palette by vm_scene_set_tone_reloads_the_palette_for_the_current_scene.
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(1, 3, 4));

  // Frame 1: engine_update detects the overlap and schedules the
  // trigger's script — but script_runner_update already ran earlier in this
  // same frame, so the palette hasn't changed yet.
  // Frame 2: the scheduled script now runs to completion (VM_OP_SET_SCENE_TONE
  // then VM_OP_END), reloading the palette for tone 2.
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(6, 3, 1));
}

TEST(animated_sprites_select_idle_and_moving_frames_by_direction) {
  reset_engine();
  engine_update(); // Drain the bootstrap script before selecting scene 3.
  load_scene(3);

  // The player starts facing down (direction 0), selecting idle frame 0.
  engine_update();
  ASSERT_EQ(test_mem_oam[2] & 0x03FFu, 0);

  // GB Studio direction 1 is left, which maps to compiler animation slot 3.
  vm_actor_set_direction(0, 1);
  engine_update();
  ASSERT_EQ(test_mem_oam[2] & 0x03FFu, 3);

  // Moving right updates facing to direction 2 and selects moving-right slot 5.
  test_set_keys(KEY_RIGHT);
  engine_update();
  ASSERT_EQ(test_mem_oam[2] & 0x03FFu, 5);
}

TEST(actor_vm_queries_and_collision_toggle_use_live_runtime_state) {
  reset_engine();
  engine_update(); // Drain bootstrap and spawn the scene player.

  actor_t *other = spawn_actor(0, 32, 48);
  ASSERT_NOT_NULL(other);

  ASSERT_TRUE(vm_actor_at_position(0, 0, 0));
  ASSERT_TRUE(!vm_actor_at_position(0, 1, 0));
  ASSERT_TRUE(vm_actor_is_relative(0, 1, 3)); // player is above actor 1
  ASSERT_TRUE(vm_actor_is_relative(1, 0, 0)); // actor 1 is below player
  ASSERT_TRUE(!vm_actor_is_relative(0, 99, 0));

  vm_actor_set_collisions(1, 0);
  ASSERT_TRUE(!other->collision_enabled);
  vm_actor_set_collisions(1, 1);
  ASSERT_TRUE(other->collision_enabled);
}

int main(void) {
  RUN_TEST(engine_init_schedules_bootstrap_and_enables_bg0);
  RUN_TEST(load_scene_renders_compiled_tilemap_and_tileset);
  RUN_TEST(vm_scene_set_tone_reloads_the_palette_for_the_current_scene);
  RUN_TEST(load_scene_schedules_its_start_script_after_spawning_actors);
  RUN_TEST(engine_update_cycles_scenes_when_start_is_pressed);
  RUN_TEST(textbox_dismiss_input_is_not_reused_for_movement_or_interaction);
  RUN_TEST(active_actors_move_and_destroyed_slots_are_reused);
  RUN_TEST(movement_type_patrol_paces_back_and_forth_across_its_bounds);
  RUN_TEST(movement_type_follow_chases_the_player_only_within_range);
  RUN_TEST(scene_trigger_runs_its_script_once_when_the_player_enters);
  RUN_TEST(animated_sprites_select_idle_and_moving_frames_by_direction);
  RUN_TEST(actor_vm_queries_and_collision_toggle_use_live_runtime_state);
  return TEST_REPORT();
}
