/* ASCII case-insensitive string comparison, for names the game stores in
 * mixed case (car aliases, file names). Non-ASCII bytes compare exactly. */
#ifndef STRING_CI_H
#define STRING_CI_H

/* Returns 1 when a and b are equal ignoring ASCII case. */
int recomp_strings_equal_ci(const char *a, const char *b);

#endif
