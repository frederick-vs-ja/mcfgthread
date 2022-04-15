// This file is part of MCF gthread.
// See LICENSE.TXT for licensing information.
// Copyleft 2022, LH_Mouse. All wrongs reserved.

#define __MCFGTHREAD_EXIT_C_  1
#include "exit.h"
#include "mutex.h"
#include "dtor_queue.h"
#include "win32.h"

void
__MCF__Exit(int status)
  {
    // Terminte the current process without invoking TLS callbacks.
    TerminateProcess(GetCurrentProcess(), (DWORD) status);
    __MCF_UNREACHABLE;
  }

void
_exit(int status)
  __attribute__((__alias__("__MCF__Exit")));

void
_Exit(int status)
  __attribute__((__alias__("__MCF__Exit")));

void
__MCF_quick_exit(int status)
  {
    // Invoke all callbacks that have been registered by `at_quick_exit()` in
    // reverse order.
    __MCF_dtor_queue_finalize(&__MCF_cxa_at_quick_exit_queue, &__MCF_cxa_at_quick_exit_mutex, NULL);

    // Call `_Exit(status)` in accordance with the ISO C standard.
    __MCF__Exit(status);
  }

void
quick_exit(int status)
  __attribute__((__alias__("__MCF_quick_exit"), __noreturn__));

void
__MCF_exit(int status)
  {
    // Perform global cleanup like `__cxa_finalize(NULL)`.
    // The shim library shall have registered a cleanup function for the CRT.
    __MCF_finalize_on_process_exit();

    // After the CRT has been finalized, exit.
    __MCF__Exit(status);
  }

void
exit(int status)
  __attribute__((__alias__("__MCF_exit"), __noreturn__));
