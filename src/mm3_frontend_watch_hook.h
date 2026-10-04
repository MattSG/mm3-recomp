/* The constructor's factory is in the same generated translation unit.
 * Observe its call seam as well as external calls to the wrapper. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
void mm3_frontend_watch_enter(void);
void mm3_frontend_before_call(uint32_t va);
void mm3_frontend_after_call(uint32_t va, uint32_t edi_before, uint32_t esp_before, uint32_t esi_before);
#undef RECOMP_ABI_CALL
#ifdef RECOMP_ABI_CHECK
#define RECOMP_ABI_CALL(va, fn) do { \
    uint32_t _ab = g_ebx, _as = g_esi, _ad = g_edi, _ap = g_esp; \
    mm3_frontend_before_call(va); \
    (fn)(); \
    mm3_frontend_after_call((va), _ad, _ap, _as); \
    if (g_ebx != _ab || g_esi != _as || g_edi != _ad || g_esp < _ap + 4) \
        recomp_abi_violation_log((va), _ab, _as, _ad, _ap); \
} while (0)
#else
#define RECOMP_ABI_CALL(va, fn) do { \
    uint32_t _mm3_edi = g_edi, _mm3_esp = g_esp, _mm3_esi = g_esi; \
    mm3_frontend_before_call(va); \
    (fn)(); \
    mm3_frontend_after_call((va), _mm3_edi, _mm3_esp, _mm3_esi); \
} while (0)
#endif
