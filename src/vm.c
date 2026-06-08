#include "vm.h"
#include <stdarg.h>
#include <stddef.h>

extern void vm_scene_load(UBYTE scene_index);
extern void vm_scene_set_tone(UBYTE tone);

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
