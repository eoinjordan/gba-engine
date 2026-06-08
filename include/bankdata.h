#ifndef GBA_BANKDATA_H
#define GBA_BANKDATA_H

#include "vm.h"
#include <stddef.h>

typedef struct far_ptr_t {
  UBYTE bank;
  void *ptr;
} far_ptr_t;

#define TO_FAR_PTR_T(VARNAME)                                                  \
  { .bank = 0, .ptr = (void *)&(VARNAME) }
#define TO_FAR_ARGS(TYPE, VALUE) (TYPE)(VALUE).ptr, (VALUE).bank

#ifndef BANK
#define BANK(VARNAME) 0
#endif

#ifndef BANKREF
#define BANKREF(VARNAME)
#endif

#ifndef BANKREF_EXTERN
#define BANKREF_EXTERN(VARNAME)
#endif

#ifndef SIZE
#define SIZE(VARNAME) ((UWORD)sizeof(VARNAME))
#endif

#ifndef SIZEREF
#define SIZEREF(VARNAME)
#endif

#ifndef SIZEREF_EXTERN
#define SIZEREF_EXTERN(VARNAME)
#endif

#endif
