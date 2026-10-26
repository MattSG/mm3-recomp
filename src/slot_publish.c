#include "slot_publish.h"

#if defined(_MSC_VER)
#include <intrin.h>
static uint32_t atomic_add32(volatile uint32_t *p, uint32_t v)
{
    return (uint32_t)_InterlockedExchangeAdd((volatile long *)p, (long)v);
}
static uint32_t atomic_cas32(volatile uint32_t *p, uint32_t cmp, uint32_t val)
{
    return (uint32_t)_InterlockedCompareExchange((volatile long *)p, (long)val, (long)cmp);
}
#else
static uint32_t atomic_add32(volatile uint32_t *p, uint32_t v)
{
    return __sync_fetch_and_add(p, v);
}
static uint32_t atomic_cas32(volatile uint32_t *p, uint32_t cmp, uint32_t val)
{
    return __sync_val_compare_and_swap(p, cmp, val);
}
#endif

void xbox_slot_publish(volatile uint32_t *sequence, volatile uint32_t *words, unsigned count,
                       const uint32_t *values, volatile uint32_t *owner, uint32_t owner_value)
{
    unsigned i;
    atomic_add32(sequence, 1); /* odd: unpublished */
    for (i = 0; i < count; ++i)
        atomic_cas32(&words[i], xbox_slot_read(&words[i]), values ? values[i] : 0);
    if (owner)
        *owner = owner_value;
    atomic_add32(sequence, 1); /* even: complete */
}

uint32_t xbox_slot_read(volatile uint32_t *word)
{
    return atomic_add32(word, 0);
}
