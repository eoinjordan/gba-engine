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

int main(void) {
  RUN_TEST(engine_init_schedules_bootstrap_and_enables_bg0);
  RUN_TEST(load_scene_renders_expected_border_collision_and_checker_tiles);
  RUN_TEST(vm_scene_set_tone_reloads_the_palette_for_the_current_scene);
  RUN_TEST(engine_update_cycles_scenes_when_start_is_pressed);
  RUN_TEST(active_actors_move_and_destroyed_slots_are_reused);
  return TEST_REPORT();
}
