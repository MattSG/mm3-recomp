# Removed crypto implementation claims

The old source claimed BCrypt-backed SHA-1 and exact Xbox compatibility.
The implementation actually stores portable SHA-1 state directly in its context;
the Windows-only SHA_INTERNAL type and BCrypt link pragma had no users.
A captured approximately 0.1-second 768-bit exponentiation estimate was also
removed. It is not a portable performance contract.

Removed unused wrapper and surrounding historical notes:

```c
/* SHA-1 / RC4 / HMAC below are portable software implementations, so the
 * only platform dependency is BCrypt's types in the (unused) SHA_INTERNAL
 * struct. Keep it Windows-only. */
#if defined(_WIN32)
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

/* ============================================================================
 * SHA-1
 *
 * Xbox SHA-1 is standard FIPS 180-1 SHA-1. We use BCrypt for correctness.
 * The XBOX_SHA_CONTEXT is our own structure (defined in kernel.h).
 * We store the BCrypt hash handle inside it.
 * ============================================================================ */

/* We use a wrapper since XBOX_SHA_CONTEXT may not be large enough for
 * BCrypt's internal state. Store BCrypt handles in a separate mapping. */

#if defined(_WIN32)
typedef struct _SHA_INTERNAL {
    BCRYPT_ALG_HANDLE  hAlg;
    BCRYPT_HASH_HANDLE hHash;
    UCHAR              hashObject[256]; /* BCrypt hash object buffer */
} SHA_INTERNAL;
#endif

/*
 * Since XBOX_SHA_CONTEXT is only 92 bytes (State[5] + Count[2] + Buffer[64]),
 * we can't fit a BCrypt handle in it. We use a simple software SHA-1 instead.
 * This also ensures byte-exact compatibility with the Xbox implementation.
 */

/* Software SHA-1 implementation - matches Xbox/FIPS exactly */

```
