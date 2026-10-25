/* Environment switches for optional features.
 *
 * A switch is on when its variable is set and not "0". Unset, it takes the
 * default. A required feature is always on. The variable is read once, on
 * the first question, and the answer is kept in the caller's cache slot:
 * 0 unread, 1 off, 2 on. */
#ifndef FEATURE_SWITCH_H
#define FEATURE_SWITCH_H

int feature_switch_on(const char *env, int default_on, int required, int *cache);

#endif
