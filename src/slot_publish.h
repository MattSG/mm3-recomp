/* Lock-free publication of a slot's words to another thread.
 *
 * A writer makes a slot's sequence odd, writes its words with compare-and-swap,
 * sets the slot's owner, and makes the sequence even again. A reader takes the
 * sequence, reads a word, and trusts the result only when the sequence is even
 * and unchanged afterwards. Nothing waits and nothing can tear a result. */
#ifndef SLOT_PUBLISH_H
#define SLOT_PUBLISH_H

#include <stdint.h>

/* Publishes count words (values, or zeros when values is NULL) and then sets
 * *owner to owner_value, between two sequence increments. owner may be NULL. */
void xbox_slot_publish(volatile uint32_t *sequence, volatile uint32_t *words, unsigned count,
                       const uint32_t *values, volatile uint32_t *owner, uint32_t owner_value);

/* Reads a word atomically, with sequential consistency. */
uint32_t xbox_slot_read(volatile uint32_t *word);

#endif
