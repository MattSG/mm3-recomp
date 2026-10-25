/* Hardware write watch contract (Windows debug registers).
 *
 * Built and run by tests/mm3_runtime_helpers_regression.py.
 *
 * A watch fires once per write on the armed thread and passes the stored
 * value. It refuses a second watch while one is armed, and disarms when the
 * callback returns 0. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "host_write_watch.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "host-write-watch line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static volatile uint32_t word;
static unsigned fires;
static uint32_t last_value;

/* 0x3000 and 0x4000 end their watch; other values keep it armed. */
static int on_write(void *user, uint32_t value, uint64_t rip)
{
    (void)user; (void)rip;
    ++fires;
    last_value = value;
    return value != 0x3000u && value != 0x4000u;
}

int main(void)
{
    volatile uint32_t *field = &word;
    word = 0;

    CHECK(host_write_watch_arm(field, on_write, NULL) == 1);
    CHECK(host_write_watch_arm(field, on_write, NULL) == 0); /* one watch at a time */
    word = 0x1234u;
    CHECK(fires == 1 && last_value == 0x1234u);
    word = 0x2000u;
    CHECK(fires == 2 && last_value == 0x2000u);
    word = 0x3000u; /* callback returns 0: disarm */
    CHECK(fires == 3 && last_value == 0x3000u);
    word = 0x3001u;
    CHECK(fires == 3); /* disarmed */

    CHECK(host_write_watch_arm(field, on_write, NULL) == 1); /* re-arm after disarm */
    word = 0x4000u;
    CHECK(fires == 4 && last_value == 0x4000u);
    word = 0x5000u;
    CHECK(fires == 4);

    puts("host write watch contract passed.");
    return 0;
}
