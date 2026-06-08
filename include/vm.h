#ifndef GBA_VM_H
#define GBA_VM_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t UBYTE;
typedef uint16_t UWORD;
typedef int8_t INT8;
typedef int16_t INT16;
typedef uint8_t UINT8;
typedef uint16_t UINT16;

#define FN_ARG0 -1
#define FN_ARG1 -2
#define FN_ARG2 -3
#define FN_ARG3 -4
#define FN_ARG4 -5
#define FN_ARG5 -6
#define FN_ARG6 -7
#define FN_ARG7 -8

#define INSTRUCTION_SIZE 1
#define VM_MAX_CONTEXTS 16
#define VM_CONTEXT_STACK_SIZE 64
#define VM_HEAP_SIZE 768
#define INSTRUCTIONS_PER_QUANT 0x10
#define SCRIPT_TERMINATED 0x8000

#define RUNNER_DONE 0
#define RUNNER_IDLE 1
#define RUNNER_BUSY 2
#define RUNNER_EXCEPTION 3

#define EXCEPTION_CODE_NONE 0

#define VM_OP_END 0x00
#define VM_OP_LOAD_SCENE 0x01
#define VM_OP_SET_SCENE_TONE 0x02
#define VM_OP_WAIT 0x03

typedef void (*SCRIPT_CMD_FN)(void);

typedef struct SCRIPT_CMD {
  SCRIPT_CMD_FN fn;
  UBYTE fn_bank;
  UBYTE args_len;
} SCRIPT_CMD;

typedef struct SCRIPT_CTX {
  const UBYTE *PC;
  UBYTE bank;
  struct SCRIPT_CTX *next;
  void *update_fn;
  UBYTE update_fn_bank;
  UWORD *stack_ptr;
  UWORD *base_addr;
  UBYTE ID;
  UWORD *hthread;
  UBYTE terminated;
  UBYTE waitable;
  UBYTE lock_count;
  UBYTE flags;
  UWORD wait_frames;
} SCRIPT_CTX;

extern UWORD script_memory[VM_HEAP_SIZE +
                           (VM_MAX_CONTEXTS * VM_CONTEXT_STACK_SIZE)];
extern SCRIPT_CTX CTXS[VM_MAX_CONTEXTS];
extern SCRIPT_CTX *first_ctx;
extern SCRIPT_CTX *free_ctxs;
extern SCRIPT_CTX *old_executing_ctx;
extern SCRIPT_CTX *executing_ctx;

extern UBYTE vm_lock_state;
extern UBYTE vm_loaded_state;
extern UBYTE vm_exception_code;
extern UBYTE vm_exception_params_length;
extern UBYTE vm_exception_params_bank;
extern const void *vm_exception_params_offset;

void script_runner_init(UBYTE reset);
SCRIPT_CTX *script_execute(UBYTE bank, UBYTE *pc, UWORD *handle, UBYTE nargs,
                           ...);
UBYTE script_terminate(UBYTE ID);
UBYTE script_detach_hthread(UBYTE ID);
UBYTE script_runner_update(void);

static inline UBYTE VM_ISLOCKED(void) { return vm_lock_state != 0; }

#endif
