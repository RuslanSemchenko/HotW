
#ifndef __ASM_INLINE_I386__
#define __ASM_INLINE_I386__

#include "../qcommon/q_platform.h"

#if idx64
  #define EAX "%%rax"
  #define EBX "%%rbx"
  #define ESP "%%rsp"
  #define EDI "%%rdi"
#else
  #define EAX "%%eax"
  #define EBX "%%ebx"
  #define ESP "%%esp"
  #define EDI "%%edi"
#endif

#endif
