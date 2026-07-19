#include "vm.h"
#include "text.h"
#include <stdarg.h>
#include <stddef.h>

extern void vm_scene_load(UBYTE scene_index);
extern void vm_scene_set_tone(UBYTE tone);
extern void textbox_open(const char *text);
extern bool textbox_update(void);
extern UWORD vm_get_keys(void);
extern void vm_actor_set_position(UBYTE actor, UBYTE x, UBYTE y);
extern void vm_actor_move_relative(UBYTE actor, INT8 dx, INT8 dy);
extern void vm_actor_set_direction(UBYTE actor, UBYTE dir);
extern void vm_actor_set_hidden(UBYTE actor, UBYTE hidden);
extern void vm_actor_set_collisions(UBYTE actor, UBYTE enabled);
extern bool vm_actor_at_position(UBYTE actor, UBYTE x, UBYTE y);
extern bool vm_actor_is_relative(UBYTE actor, UBYTE other_actor, UBYTE dir);

UWORD script_memory[VM_HEAP_SIZE + (VM_MAX_CONTEXTS * VM_CONTEXT_STACK_SIZE)];
SCRIPT_CTX CTXS[VM_MAX_CONTEXTS];
SCRIPT_CTX *first_ctx;
SCRIPT_CTX *free_ctxs;
SCRIPT_CTX *old_executing_ctx;
SCRIPT_CTX *executing_ctx;

UBYTE vm_lock_state;
UBYTE vm_loaded_state;
UBYTE vm_exception_code;
UBYTE vm_exception_params_length;
UBYTE vm_exception_params_bank;
const void *vm_exception_params_offset;

INT16 vm_variables[VM_VARIABLE_COUNT];

// Small, fast xorshift16-style PRNG for VM_OP_RANDOM. Not cryptographic —
// just needs to look random enough for game behaviour (item drops, enemy
// choices, etc) and to be cheap on an ARM7TDMI with no hardware divider.
static UWORD vm_rng_state = 0xF491u;

void vm_seed_random(UWORD seed) {
  // A zero state is a fixed point for xorshift — never let it stick there.
  vm_rng_state = seed != 0 ? seed : 0xF491u;
}

static UWORD vm_random_next(void) {
  UWORD x = vm_rng_state;
  x ^= (UWORD)(x << 7);
  x ^= (UWORD)(x >> 9);
  x ^= (UWORD)(x << 8);
  vm_rng_state = x;
  return x;
}

// Jump/branch targets are encoded as a little-endian signed 16-bit relative
// offset, measured from the instruction immediately following the offset
// itself (i.e. from `ctx->PC` *after* this returns).
static INT16 vm_read_offset(SCRIPT_CTX *ctx) {
  UBYTE lo = *ctx->PC++;
  UBYTE hi = *ctx->PC++;
  return (INT16)(UWORD)(((UWORD)hi << 8) | (UWORD)lo);
}

static UWORD *context_stack_base(UBYTE index) {
  return &script_memory[VM_HEAP_SIZE + (index * VM_CONTEXT_STACK_SIZE)];
}

void script_runner_init(UBYTE reset) {
  first_ctx = NULL;
  free_ctxs = &CTXS[0];
  old_executing_ctx = NULL;
  executing_ctx = NULL;
  vm_lock_state = 0;
  vm_loaded_state = 0;
  vm_exception_code = EXCEPTION_CODE_NONE;
  vm_exception_params_length = 0;
  vm_exception_params_bank = 0;
  vm_exception_params_offset = NULL;

  if (reset) {
    for (uint16_t i = 0; i < VM_HEAP_SIZE; i++) {
      script_memory[i] = 0;
    }
    for (uint16_t i = 0; i < VM_VARIABLE_COUNT; i++) {
      vm_variables[i] = 0;
    }
  }

  for (UBYTE i = 0; i < VM_MAX_CONTEXTS; i++) {
    SCRIPT_CTX *ctx = &CTXS[i];
    ctx->PC = NULL;
    ctx->bank = 0;
    ctx->next = (i + 1 < VM_MAX_CONTEXTS) ? &CTXS[i + 1] : NULL;
    ctx->update_fn = NULL;
    ctx->update_fn_bank = 0;
    ctx->base_addr = context_stack_base(i);
    ctx->stack_ptr = ctx->base_addr;
    ctx->ID = i + 1;
    ctx->hthread = NULL;
    ctx->terminated = 1;
    ctx->waitable = 0;
    ctx->lock_count = 0;
    ctx->flags = 0;
    ctx->wait_frames = 0;
  }
}

SCRIPT_CTX *script_execute(UBYTE bank, UBYTE *pc, UWORD *handle, UBYTE nargs,
                           ...) {
  if (free_ctxs == NULL) {
    return NULL;
  }

  SCRIPT_CTX *ctx = free_ctxs;
  free_ctxs = ctx->next;
  ctx->next = first_ctx;
  first_ctx = ctx;

  ctx->PC = pc;
  ctx->bank = bank;
  ctx->stack_ptr = ctx->base_addr;
  ctx->hthread = handle;
  ctx->terminated = 0;
  ctx->waitable = 1;
  ctx->lock_count = 0;
  ctx->flags = 0;
  ctx->wait_frames = 0;

  if (handle != NULL) {
    *handle = ctx->ID;
  }

  va_list args;
  va_start(args, nargs);
  for (UBYTE i = 0; i < nargs && i < VM_CONTEXT_STACK_SIZE; i++) {
    *ctx->stack_ptr++ = (UWORD)va_arg(args, int);
  }
  va_end(args);

  return ctx;
}

UBYTE script_terminate(UBYTE ID) {
  SCRIPT_CTX **cursor = &first_ctx;
  while (*cursor != NULL) {
    SCRIPT_CTX *ctx = *cursor;
    if (ctx->ID == ID) {
      *cursor = ctx->next;
      ctx->terminated = 1;
      if (ctx->hthread != NULL) {
        *ctx->hthread = SCRIPT_TERMINATED;
      }
      ctx->next = free_ctxs;
      free_ctxs = ctx;
      return 0;
    }
    cursor = &ctx->next;
  }

  return 1;
}

UBYTE script_detach_hthread(UBYTE ID) {
  for (SCRIPT_CTX *ctx = first_ctx; ctx != NULL; ctx = ctx->next) {
    if (ctx->ID == ID) {
      ctx->hthread = NULL;
      return 0;
    }
  }

  return 1;
}

UBYTE script_runner_update(void) {
  SCRIPT_CTX *ctx = first_ctx;

  while (ctx != NULL) {
    executing_ctx = ctx;

    if (ctx->wait_frames > 0) {
      ctx->wait_frames--;
      ctx = ctx->next;
      continue;
    }

    // update_fn: a context-local callback (e.g. textbox_update) that blocks
    // script execution until it returns true. Set by opcodes that need to
    // wait for an async condition (player input, animation end, etc.).
    if (ctx->update_fn != NULL) {
      typedef bool (*update_fn_t)(void);
      update_fn_t fn = (update_fn_t)ctx->update_fn;
      if (!fn()) {
        ctx = ctx->next;
        continue;
      }
      ctx->update_fn = NULL;
    }

    for (UBYTE i = 0; i < INSTRUCTIONS_PER_QUANT && ctx->PC != NULL; i++) {
      UBYTE opcode = *ctx->PC++;

      switch (opcode) {
      case VM_OP_END:
        script_terminate(ctx->ID);
        executing_ctx = NULL;
        return first_ctx == NULL ? RUNNER_DONE : RUNNER_BUSY;

      case VM_OP_LOAD_SCENE:
        vm_scene_load(*ctx->PC++);
        break;

      case VM_OP_SET_SCENE_TONE:
        vm_scene_set_tone(*ctx->PC++);
        break;

      case VM_OP_WAIT:
        ctx->wait_frames = *ctx->PC++;
        i = INSTRUCTIONS_PER_QUANT;
        break;

      // -----------------------------------------------------------------
      // Variables & math
      // -----------------------------------------------------------------
      case VM_OP_SET_CONST: {
        UBYTE var = *ctx->PC++;
        UBYTE value = *ctx->PC++;
        vm_variables[var] = (INT16)value;
        break;
      }

      case VM_OP_COPY_VAR: {
        UBYTE dst = *ctx->PC++;
        UBYTE src = *ctx->PC++;
        vm_variables[dst] = vm_variables[src];
        break;
      }

      case VM_OP_ADD_CONST: {
        UBYTE var = *ctx->PC++;
        UBYTE value = *ctx->PC++;
        vm_variables[var] = (INT16)(vm_variables[var] + (INT16)value);
        break;
      }

      case VM_OP_SUB_CONST: {
        UBYTE var = *ctx->PC++;
        UBYTE value = *ctx->PC++;
        vm_variables[var] = (INT16)(vm_variables[var] - (INT16)value);
        break;
      }

      case VM_OP_ADD_VAR: {
        UBYTE dst = *ctx->PC++;
        UBYTE src = *ctx->PC++;
        vm_variables[dst] = (INT16)(vm_variables[dst] + vm_variables[src]);
        break;
      }

      case VM_OP_SUB_VAR: {
        UBYTE dst = *ctx->PC++;
        UBYTE src = *ctx->PC++;
        vm_variables[dst] = (INT16)(vm_variables[dst] - vm_variables[src]);
        break;
      }

      case VM_OP_RANDOM: {
        UBYTE var = *ctx->PC++;
        UBYTE min_value = *ctx->PC++;
        UBYTE max_value = *ctx->PC++;
        UBYTE lo = min_value < max_value ? min_value : max_value;
        UBYTE hi = min_value < max_value ? max_value : min_value;
        UWORD range = (UWORD)(hi - lo) + 1;
        vm_variables[var] = (INT16)(lo + (vm_random_next() % range));
        break;
      }

      // -----------------------------------------------------------------
      // Control flow — relative jumps (see vm_read_offset / vm.h)
      // -----------------------------------------------------------------
      case VM_OP_JUMP: {
        INT16 offset = vm_read_offset(ctx);
        ctx->PC += offset;
        break;
      }

      case VM_OP_IF_VAR_EQ_CONST:
      case VM_OP_IF_VAR_GT_CONST:
      case VM_OP_IF_VAR_LT_CONST: {
        UBYTE var = *ctx->PC++;
        UBYTE value = *ctx->PC++;
        INT16 offset = vm_read_offset(ctx);
        bool branch = false;

        INT16 current = vm_variables[var];
        switch (opcode) {
        case VM_OP_IF_VAR_EQ_CONST:
          branch = current == (INT16)value;
          break;
        case VM_OP_IF_VAR_GT_CONST:
          branch = current > (INT16)value;
          break;
        case VM_OP_IF_VAR_LT_CONST:
          branch = current < (INT16)value;
          break;
        }

        if (branch) {
          ctx->PC += offset;
        }
        break;
      }

      case VM_OP_SHOW_TEXT: {
        // Inline NUL-terminated string follows the opcode in the bytecode.
        // Advance PC past the string (including the NUL terminator) so
        // execution resumes at the next opcode after the text.
        const char *str = (const char *)ctx->PC;
        while (*ctx->PC++);

        static char textbox_buf[256];
        text_format_variables(str, textbox_buf, sizeof(textbox_buf));
        textbox_open(textbox_buf);

        // Block this context until the player presses A (textbox_update
        // returns true). Yield the rest of this quantum so the frame can
        // render before the runner polls again.
        ctx->update_fn = (void *)textbox_update;
        i = INSTRUCTIONS_PER_QUANT;
        break;
      }

      // -----------------------------------------------------------------
      // Input & actors
      // -----------------------------------------------------------------
      case VM_OP_IF_INPUT: {
        UBYTE lo = *ctx->PC++;
        UBYTE hi = *ctx->PC++;
        UWORD mask = (UWORD)(((UWORD)hi << 8) | (UWORD)lo);
        INT16 offset = vm_read_offset(ctx);
        if ((vm_get_keys() & mask) != 0) {
          ctx->PC += offset;
        }
        break;
      }

      case VM_OP_ACTOR_SET_POS: {
        UBYTE actor = *ctx->PC++;
        UBYTE x = *ctx->PC++;
        UBYTE y = *ctx->PC++;
        vm_actor_set_position(actor, x, y);
        break;
      }

      case VM_OP_ACTOR_MOVE_REL: {
        UBYTE actor = *ctx->PC++;
        INT8 dx = (INT8)*ctx->PC++;
        INT8 dy = (INT8)*ctx->PC++;
        vm_actor_move_relative(actor, dx, dy);
        break;
      }

      case VM_OP_ACTOR_SET_DIR: {
        UBYTE actor = *ctx->PC++;
        UBYTE dir = *ctx->PC++;
        vm_actor_set_direction(actor, dir);
        break;
      }

      case VM_OP_ACTOR_SET_HIDDEN: {
        UBYTE actor = *ctx->PC++;
        UBYTE hidden = *ctx->PC++;
        vm_actor_set_hidden(actor, hidden);
        break;
      }

      case VM_OP_ACTOR_SET_COLLISIONS: {
        UBYTE actor = *ctx->PC++;
        UBYTE enabled = *ctx->PC++;
        vm_actor_set_collisions(actor, enabled);
        break;
      }

      case VM_OP_IF_ACTOR_AT_POS: {
        UBYTE actor = *ctx->PC++;
        UBYTE x = *ctx->PC++;
        UBYTE y = *ctx->PC++;
        INT16 offset = vm_read_offset(ctx);
        if (vm_actor_at_position(actor, x, y)) {
          ctx->PC += offset;
        }
        break;
      }

      case VM_OP_IF_ACTOR_RELATIVE: {
        UBYTE actor = *ctx->PC++;
        UBYTE other_actor = *ctx->PC++;
        UBYTE direction = *ctx->PC++;
        INT16 offset = vm_read_offset(ctx);
        if (vm_actor_is_relative(actor, other_actor, direction)) {
          ctx->PC += offset;
        }
        break;
      }

      default:
        vm_exception_code = opcode;
        script_terminate(ctx->ID);
        executing_ctx = NULL;
        return RUNNER_EXCEPTION;
      }
    }

    ctx = ctx->next;
  }

  executing_ctx = NULL;
  return first_ctx == NULL ? RUNNER_IDLE : RUNNER_BUSY;
}
