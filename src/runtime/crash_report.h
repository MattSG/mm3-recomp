/* Unhandled-exception report for a recompiled guest.
 *
 * xbox_CrashReport prints the host fault, the native backtrace relative to the
 * module, the guest's registers and the saved-EBP caller chain. A title's own
 * fields (its object layouts, its heap) are added by its handler after this
 * returns. Install it with SetUnhandledExceptionFilter and return the result. */
#ifndef XBOX_CRASH_REPORT_H
#define XBOX_CRASH_REPORT_H

#include "platform/xbox_winnt.h"

LONG xbox_CrashReport(EXCEPTION_POINTERS *info);

#endif
