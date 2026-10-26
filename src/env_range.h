/* Numeric environment switches that must lie in a range.
 *
 * An unset variable gives the fallback. A value that does not parse, has
 * trailing text, or lies outside [lo, hi] is reported on stderr under tag and
 * gives the fallback. */
#ifndef ENV_RANGE_H
#define ENV_RANGE_H

float recomp_env_float_in_range(const char *tag, const char *name, float fallback,
                                float lo, float hi);
long recomp_env_long_in_range(const char *tag, const char *name, long fallback,
                              long lo, long hi);

#endif
