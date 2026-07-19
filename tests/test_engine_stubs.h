#ifndef TEST_ENGINE_STUBS_H
#define TEST_ENGINE_STUBS_H

#include <stdbool.h>
#include <stdint.h>

void test_reset_environment(void);
void test_set_keys(uint16_t keys);
bool textbox_is_open(void);

#endif
