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

static uint16_t oam_attr0(unsigned index) { return test_mem_oam[index * 4u]; }
static uint16_t oam_attr1(unsigned index) { return test_mem_oam[index * 4u + 1u]; }
static uint16_t oam_attr2(unsigned index) { return test_mem_oam[index * 4u + 2u]; }

// Studio compiles iso_movement animation slots into the engine's standard
// down/right/up/left table as SW, SE, NE, NW, followed by moving variants.
enum {
  STUDIO_ISO_IDLE_SW = 0,
  STUDIO_ISO_IDLE_SE = 1,
  STUDIO_ISO_IDLE_NE = 2,
  STUDIO_ISO_IDLE_NW = 3,
  STUDIO_ISO_MOVING_SW = 4,
  STUDIO_ISO_MOVING_SE = 5,
  STUDIO_ISO_MOVING_NE = 6,
  STUDIO_ISO_MOVING_NW = 7,
};

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

TEST(sprite_tiles_copy_to_obj_vram_as_gba_safe_halfwords) {
  reset_engine();
  engine_update();
  load_scene(3);

  // OBJ VRAM begins at byte offset 0x10000 in test_mem_vram. These values
  // prove adjacent source bytes remain distinct rather than an 8-bit VRAM
  // write mirroring the second byte across each halfword.
  ASSERT_EQ(test_mem_vram[0x10000u / 2u], 0x3412u);
  ASSERT_EQ(test_mem_vram[0x10000u / 2u + 1u], 0x7856u);
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

TEST(isometric_scene_uses_independent_background_stride_and_centred_projection) {
  reset_engine();
  engine_update(); // Drain bootstrap before explicitly loading scene 5.
  load_scene(5);

  uint16_t *map = screenblock(28);
  ASSERT_EQ(map[0], 1);
  ASSERT_EQ(map[29], 2);
  ASSERT_EQ(map[32], 3); // Source row 1 starts at index 30, not logical width 8.

  // Isolate the player, then place it at the demo's (4,5) start cell.
  for (uint8_t actor = 1; actor <= 4; actor++) {
    vm_actor_set_hidden(actor, 1);
  }
  vm_actor_set_position(0, 4, 5);
  engine_update();

  // 8x7 at 32x16 projects to 240x120 and is vertically centred in the
  // 240x160 background. The 8x8 test player is bottom-centred on the tile.
  ASSERT_EQ(test_reg_bg0hofs, 0);
  ASSERT_EQ(test_reg_bg0vofs, 0);
  ASSERT_EQ(oam_attr1(0) & 0x01FFu, 92);
  ASSERT_EQ(oam_attr0(0) & 0x00FFu, 92);
}

TEST(isometric_camera_scrolls_background_and_projected_actor_together) {
  reset_engine();
  engine_update();
  load_scene(6);
  vm_actor_set_position(0, 7, 0);
  engine_update();

  // The 8x8 diamond/background are 256px wide, so the rightmost player cell
  // clamps the 240px viewport to the shared 16px maximum scroll.
  ASSERT_EQ(test_reg_bg0hofs, 16);
  ASSERT_EQ(test_reg_bg0vofs, 0);
  ASSERT_EQ(oam_attr1(0) & 0x01FFu, 220);
  ASSERT_EQ(oam_attr0(0) & 0x00FFu, 72);
}

TEST(isometric_metasprites_accumulate_deltas_use_8x16_oam_and_anchor_feet) {
  reset_engine();
  engine_update();
  load_scene(5);

  // Runtime actor 3 uses the two-object 16x16 fixture at logical cell (3,3).
  vm_actor_set_hidden(0, 1);
  vm_actor_set_hidden(1, 1);
  vm_actor_set_hidden(2, 1);
  vm_actor_set_hidden(4, 1);
  engine_update();

  // Compiler deltas +8,-8 accumulate to x=8,0. The frame is centred at
  // projected x=112 and its bottom lands on the tile centre at y=76.
  ASSERT_EQ(oam_attr1(0) & 0x01FFu, 112);
  ASSERT_EQ(oam_attr1(1) & 0x01FFu, 104);
  ASSERT_EQ(oam_attr0(0) & 0x00FFu, 60);
  ASSERT_EQ(oam_attr0(1) & 0x00FFu, 60);
  ASSERT_EQ(oam_attr0(0) & 0xC000u, 0x8000u); // vertical size-0 = 8x16
  ASSERT_EQ(oam_attr0(1) & 0xC000u, 0x8000u);
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, 8);
  ASSERT_EQ(oam_attr2(1) & 0x03FFu, 10);
}

TEST(isometric_depth_emits_nearest_actor_first_and_applies_height) {
  reset_engine();
  engine_update();
  load_scene(5);

  // Keep actors at depth 10 and 11 only. Lower OAM index wins sprite overlap,
  // so depth 11 must be emitted first.
  vm_actor_set_hidden(0, 1);
  vm_actor_set_hidden(3, 1);
  vm_actor_set_hidden(4, 1);
  engine_update();
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, 1); // actor (6,5), direction right
  ASSERT_EQ(oam_attr2(1) & 0x03FFu, 3); // actor (5,5), direction left

  // iso_z=1 raises actor 4 by one projected tile height.
  vm_actor_set_hidden(1, 1);
  vm_actor_set_hidden(2, 1);
  vm_actor_set_hidden(4, 0);
  engine_update();
  ASSERT_EQ(oam_attr1(0) & 0x01FFu, 108);
  ASSERT_EQ(oam_attr0(0) & 0x00FFu, 36);
}

TEST(isometric_input_steps_once_then_repeats_without_skipping_cells) {
  reset_engine();
  engine_update();
  load_scene(5);
  for (uint8_t actor = 1; actor <= 4; actor++) {
    vm_actor_set_hidden(actor, 1);
  }
  vm_actor_set_position(0, 3, 3);

  test_set_keys(KEY_UP);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 3, 2));
  // tile_y-- projects NE and selects the moving-NE compiled animation slot.
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, STUDIO_ISO_MOVING_NE);

  for (uint8_t frame = 0; frame < 5; frame++) {
    engine_update();
    ASSERT_TRUE(vm_actor_at_position(0, 3, 2));
  }
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 3, 1));

  test_set_keys(0);
  engine_update();
  test_set_keys(KEY_LEFT);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 2, 1));
  // tile_x-- projects NW and selects the moving-NW compiled animation slot.
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, STUDIO_ISO_MOVING_NW);
}

TEST(isometric_movement_selects_all_studio_compiled_direction_slots) {
  reset_engine();
  engine_update();
  load_scene(5);
  for (uint8_t actor = 1; actor <= 4; actor++) {
    vm_actor_set_hidden(actor, 1);
  }
  vm_actor_set_position(0, 3, 4);

  test_set_keys(KEY_UP);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 3, 3));
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, STUDIO_ISO_MOVING_NE);

  test_set_keys(KEY_DOWN);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 3, 4));
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, STUDIO_ISO_MOVING_SW);

  test_set_keys(KEY_LEFT);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 2, 4));
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, STUDIO_ISO_MOVING_NW);

  test_set_keys(KEY_RIGHT);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 3, 4));
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, STUDIO_ISO_MOVING_SE);
}

TEST(isometric_collision_blocks_each_grid_step_and_map_edges) {
  reset_engine();
  engine_update();
  load_scene(5);

  // A scripted/non-player velocity larger than one must still stop on the
  // intermediate wall instead of tunnelling from x=2 to x=5.
  actor_t *runner = spawn_actor(0, 2, 3);
  ASSERT_NOT_NULL(runner);
  runner->vel_x = 3;
  runner->vel_y = 0;
  engine_update();
  ASSERT_EQ(runner->x, 3);
  ASSERT_EQ(runner->y, 3);

  vm_actor_set_position(0, 3, 3);

  // Logical cell (4,3) is solid.
  test_set_keys(KEY_RIGHT);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 3, 3));

  test_set_keys(0);
  engine_update();
  vm_actor_set_position(0, 0, 0);
  test_set_keys(KEY_LEFT);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 0, 0));
}

TEST(isometric_interaction_requires_cardinal_adjacency) {
  reset_engine();
  engine_update();
  load_scene(5);

  // (4,4) is diagonally adjacent to actor (5,5), not cardinally adjacent.
  vm_actor_set_position(0, 4, 4);
  test_set_keys(KEY_A);
  engine_update();
  test_set_keys(0);
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(1, 3, 4));

  // From (4,5), actor (5,5) is exactly one grid edge away. Its tone-1 script
  // must win; the tone-2 actor at (6,5) is two cells away.
  vm_actor_set_position(0, 4, 5);
  test_set_keys(KEY_A);
  engine_update();
  test_set_keys(0);
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(2, 5, 8));
}

TEST(isometric_triggers_fire_on_entry_and_can_transition_to_topdown) {
  reset_engine();
  engine_update();
  load_scene(5);
  vm_actor_set_position(0, 3, 3);

  // Step onto trigger (3,2), then allow its queued script to run.
  test_set_keys(KEY_UP);
  engine_update();
  test_set_keys(0);
  engine_update();
  ASSERT_EQ(test_mem_palette[0], RGB15(4, 1, 5));

  load_scene(5);
  vm_actor_set_position(0, 7, 5);
  test_set_keys(KEY_DOWN);
  engine_update();
  ASSERT_TRUE(vm_actor_at_position(0, 7, 6));
  test_set_keys(0);
  engine_update(); // Trigger script loads top-down scene 3.

  // Top-down destination operands are tile coordinates and become pixels.
  ASSERT_TRUE(vm_actor_at_position(0, 16, 24));
  ASSERT_EQ(test_reg_bg0hofs, 0);
  ASSERT_EQ(test_reg_bg0vofs, 0);
  ASSERT_EQ(oam_attr1(0) & 0x01FFu, 16);
  ASSERT_EQ(oam_attr0(0) & 0x00FFu, 24);
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, 1); // direction right
}

TEST(scene_load_at_keeps_isometric_destination_in_grid_units) {
  reset_engine();
  engine_update();

  vm_scene_load_at(5, 4, 5, 1);
  ASSERT_TRUE(vm_actor_at_position(0, 4, 5));
  for (uint8_t actor = 1; actor <= 4; actor++) {
    vm_actor_set_hidden(actor, 1);
  }
  engine_update();

  ASSERT_EQ(oam_attr1(0) & 0x01FFu, 92);
  ASSERT_EQ(oam_attr0(0) & 0x00FFu, 92);
  ASSERT_EQ(oam_attr2(0) & 0x03FFu, 3); // direction left
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
  RUN_TEST(sprite_tiles_copy_to_obj_vram_as_gba_safe_halfwords);
  RUN_TEST(actor_vm_queries_and_collision_toggle_use_live_runtime_state);
  RUN_TEST(isometric_scene_uses_independent_background_stride_and_centred_projection);
  RUN_TEST(isometric_camera_scrolls_background_and_projected_actor_together);
  RUN_TEST(isometric_metasprites_accumulate_deltas_use_8x16_oam_and_anchor_feet);
  RUN_TEST(isometric_depth_emits_nearest_actor_first_and_applies_height);
  RUN_TEST(isometric_input_steps_once_then_repeats_without_skipping_cells);
  RUN_TEST(isometric_movement_selects_all_studio_compiled_direction_slots);
  RUN_TEST(isometric_collision_blocks_each_grid_step_and_map_edges);
  RUN_TEST(isometric_interaction_requires_cardinal_adjacency);
  RUN_TEST(isometric_triggers_fire_on_entry_and_can_transition_to_topdown);
  RUN_TEST(scene_load_at_keeps_isometric_destination_in_grid_units);
  return TEST_REPORT();
}
