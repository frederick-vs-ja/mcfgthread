/* This file is part of MCF Gthread.
 * See LICENSE.TXT for licensing information.
 * Copyleft 2022, LH_Mouse. All wrongs reserved.  */

#include "precompiled.i"
#define __MCF_THREAD_IMPORT  __MCF_DLLEXPORT
#define __MCF_THREAD_INLINE  __MCF_DLLEXPORT
#include "thread.h"
#include "mutex.h"
#include "cond.h"
#include "xglobals.i"

static __attribute__((__force_align_arg_pointer__))
DWORD
__stdcall
do_win32_thread_thunk(LPVOID param)
  {
    __MCF_SEH_DEFINE_TERMINATE_FILTER;
    _MCF_thread* const self = param;

    /* `__tid` has to be set in case that this thread begins execution before
     * `CreateThread()` returns from the other thread.  */
    _MCF_atomic_store_32_rlx(&(self->__tid), (int32_t) _MCF_thread_self_tid());

#if defined(__i386__) || defined(__amd64__)
    /* Set x87 precision to 64-bit mantissa (GNU `long double` format).  */
    __asm__ volatile ("fninit");
#endif  /* x86  */

    /* Attach the thread and execute the user-defined procedure.  */
    TlsSetValue(__MCF_g->__tls_index, self);
    self->__proc(self);
    return 0;
  }

__MCF_DLLEXPORT
_MCF_thread*
_MCF_thread_new_aligned(_MCF_thread_procedure* proc, size_t align, const void* data_opt, size_t size)
  {
    /* Validate arguments.  */
    if(!proc)
      return __MCF_win32_error_p(ERROR_INVALID_PARAMETER, NULL);

    if(align & (align - 1))
      return __MCF_win32_error_p(ERROR_NOT_SUPPORTED, NULL);

    if(align >= 0x10000000U)
      return __MCF_win32_error_p(ERROR_NOT_SUPPORTED, NULL);

    if(size >= 0x7FF00000U)
      return __MCF_win32_error_p(ERROR_ARITHMETIC_OVERFLOW, NULL);

    /* Allocate and initialize the thread control structure.  */
    uint32_t align_fixup = 0;
    if(__MCF_THREAD_DATA_ALIGNMENT > MEMORY_ALLOCATION_ALIGNMENT)
      align_fixup = __MCF_THREAD_DATA_ALIGNMENT - MEMORY_ALLOCATION_ALIGNMENT;

    _MCF_thread* thrd = __MCF_malloc_0(sizeof(_MCF_thread) + size + align_fixup);
    if(!thrd)
      return __MCF_win32_error_p(ERROR_NOT_ENOUGH_MEMORY, NULL);

    _MCF_atomic_store_32_rlx(thrd->__nref, 2);
    thrd->__proc = proc;

    thrd->__data_ptr = thrd->__data_storage;
    if(align_fixup != 0)
      thrd->__data_ptr = (char*) ((uintptr_t) (thrd->__data_ptr - 1) | (__MCF_THREAD_DATA_ALIGNMENT - 1)) + 1;

    if(data_opt)
      __MCF_mcopy(thrd->__data_ptr, data_opt, size);

    /* Create the thread now.  */
    DWORD tid;
    thrd->__handle = CreateThread(NULL, 0, do_win32_thread_thunk, thrd, 0, &tid);
    if(thrd->__handle == NULL) {
      __MCF_mfree(thrd);
      return NULL;
    }

    /* Set `__tid` in case `CreateThread()` returns before the new thread begins
     * execution. It may be overwritten by the new thread with the same value.  */
    _MCF_atomic_store_32_rlx(&(thrd->__tid), (int32_t) tid);
    return thrd;
  }

__MCF_DLLEXPORT
void
_MCF_thread_drop_ref_nonnull(_MCF_thread* thrd)
  {
    int32_t old_ref = _MCF_atomic_xsub_32_arl(thrd->__nref, 1);
    __MCF_ASSERT(old_ref > 0);
    if(old_ref != 1)
      return;

    /* The main thread structure is allocated statically and must not be freed.  */
    if(thrd == __MCF_g->__main_thread)
      return;

    __MCF_close_handle(thrd->__handle);
    __MCF_mfree(thrd);
  }

__MCF_DLLEXPORT
void
_MCF_thread_exit(void)
  {
    ExitThread(0);
    __MCF_UNREACHABLE;
  }

__MCF_DLLEXPORT
int
_MCF_thread_wait(const _MCF_thread* thrd_opt, const int64_t* timeout_opt)
  {
    if(!thrd_opt)
      return -1;

    __MCF_winnt_timeout nt_timeout;
    __MCF_initialize_winnt_timeout_v2(&nt_timeout, timeout_opt);

    NTSTATUS status = NtWaitForSingleObject(thrd_opt->__handle, false, nt_timeout.__li);
    __MCF_ASSERT_NT(status);
    return (status != STATUS_WAIT_0) ? -1 : 0;
  }

__MCF_DLLEXPORT
_MCF_thread_priority
_MCF_thread_get_priority(const _MCF_thread* thrd_opt)
  {
    HANDLE handle = thrd_opt ? thrd_opt->__handle : GetCurrentThread();
    return (_MCF_thread_priority) GetThreadPriority(handle);
  }

__MCF_DLLEXPORT
int
_MCF_thread_set_priority(_MCF_thread* thrd_opt, _MCF_thread_priority priority)
  {
    HANDLE handle = thrd_opt ? thrd_opt->__handle : GetCurrentThread();
    BOOL succ = SetThreadPriority(handle, (int8_t) priority);
    return !succ ? -1 : 0;
  }

__MCF_DLLEXPORT
_MCF_thread*
_MCF_thread_self(void)
  {
    return TlsGetValue(__MCF_g->__tls_index);
  }

__MCF_DLLEXPORT
void
_MCF_yield(void)
  {
    SwitchToThread();
  }

static
BOOL
__stdcall
do_handle_interrupt(DWORD type)
  {
    (void) type;
    _MCF_cond_signal_all(__MCF_g->__interrupt_cond);
    return false;
  }

static inline
void
do_handler_cleanup(BOOL* added)
  {
    if(*added)
      SetConsoleCtrlHandler(do_handle_interrupt, false);
  }

__MCF_DLLEXPORT
int
_MCF_sleep(const int64_t* timeout_opt)
  {
    /* Set a handler to receive Ctrl-C notifications.  */
    BOOL added __attribute__((__cleanup__(do_handler_cleanup))) = false;
    added = SetConsoleCtrlHandler(do_handle_interrupt, true);

    int err = _MCF_cond_wait(__MCF_g->__interrupt_cond, NULL, NULL, 0, timeout_opt);
    return err ^ -1;
  }

__MCF_DLLEXPORT
void
_MCF_sleep_noninterruptible(const int64_t* timeout_opt)
  {
    __MCF_winnt_timeout nt_timeout;
    __MCF_initialize_winnt_timeout_v2(&nt_timeout, timeout_opt);

    NTSTATUS status = NtDelayExecution(false, nt_timeout.__li);
    __MCF_ASSERT_NT(status);
  }
