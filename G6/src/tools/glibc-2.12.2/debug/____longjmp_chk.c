/* Stub to make building happy -- no fortify checks, but
   it at least does the correct thing (longjmps).  */
#include <setjmp.h>
void ____longjmp_chk (__jmp_buf env, int val)
{
  return __longjmp (env, val);
}
