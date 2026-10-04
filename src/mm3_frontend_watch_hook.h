/* The constructor's factory is in the same generated translation unit.
 * Observe its call seam as well as external calls to the wrapper. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
void mm3_frontend_watch_enter(void);
void mm3_frontend_before_call(uint32_t va);
void mm3_frontend_icall_site(uint32_t va, uint32_t site);
void mm3_frontend_after_call(uint32_t va, uint32_t edi_before, uint32_t esp_before, uint32_t esi_before);

#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(va, saved_esp, site) do { \
    uint32_t _mm3_target = (uint32_t)(va), _mm3_saved = (saved_esp); \
    uint32_t _mm3_di = g_edi, _mm3_sp = g_esp, _mm3_si = g_esi; \
    mm3_frontend_before_call(_mm3_target); \
    mm3_frontend_icall_site(_mm3_target, (site)); \
    RECOMP_ICALL_SAFE(_mm3_target, _mm3_saved); \
    mm3_frontend_after_call(_mm3_target, _mm3_di, _mm3_sp, _mm3_si); \
} while (0)
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
