#ifndef MM3_DEV_TOOLS_H
#define MM3_DEV_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Queue an existing prepared command; execution stays on the game thread.
 * Returns zero if invalid, unavailable, or another UI command is pending. */
int mm3_dev_tools_request(unsigned probe);
void mm3_dev_tools_status(char *text, unsigned size);

#ifdef __cplusplus
}
#endif
#endif
