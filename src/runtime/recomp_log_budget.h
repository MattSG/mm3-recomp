/* Per-second line budget for diagnostic logs.
 *
 * Each one-second window admits `limit` lines. Lines past that are counted as
 * suppressed, and the count of the window that just ended is reported through
 * RecompLogRoll so the caller can say how much was dropped. The budget is
 * per caller: keep one RecompLogBudget per log stream and per guest thread
 * when the stream is thread-local. */
#ifndef RECOMP_LOG_BUDGET_H
#define RECOMP_LOG_BUDGET_H

#include <stdint.h>

typedef struct {
    uint64_t second;     /* the window's start, in whole seconds */
    unsigned lines;      /* lines admitted in this window */
    unsigned suppressed; /* lines refused in this window */
} RecompLogBudget;

typedef struct {
    uint64_t second;     /* the window that ended */
    unsigned suppressed; /* lines refused in it; zero when nothing was dropped */
} RecompLogRoll;

/* Returns 1 when a line may be written at now_ms. */
static inline int recomp_log_budget_take(RecompLogBudget *budget, uint64_t now_ms,
                                         unsigned limit, RecompLogRoll *rolled)
{
    uint64_t second = now_ms / 1000;
    if (rolled) { rolled->second = budget->second; rolled->suppressed = 0; }
    if (second != budget->second) {
        if (rolled) rolled->suppressed = budget->suppressed;
        budget->second = second;
        budget->lines = budget->suppressed = 0;
    }
    if (budget->lines >= limit) {
        ++budget->suppressed;
        return 0;
    }
    ++budget->lines;
    return 1;
}

#endif
