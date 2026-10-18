#include <stdio.h>
#include <string.h>
#include "mm3_frame_limit.h"

/* stderr is captured in a file so the one-time rejection log can be counted. */
#define LOG_PATH "mm3_frame_limit_stderr.txt"

/* Not assert: the calls inside the checks must run in release builds too. */
#define CHECK(x) do { if (!(x)) { printf("%s:%d: failed: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void)
{
    static const struct { const char *value; int expected; } cases[] = {
        { NULL, 60 }, { "", 60 }, { "60", 60 }, { "59", 59 },
        { "30", 30 }, { "1", 1 }, { "0", 0 }, { "-1", 60 },
        { "61", 61 }, { "90", 90 }, { "91", 91 }, { "120", 120 }, { "165", 165 },
        { "2147483647", INT_MAX }, { "2147483648", 60 },
        { "invalid", 60 }, { "30junk", 60 }, { "59.94", 60 },
        { " 30\t ", 30 }, { "999999999999999999999999999999", 60 },
        { "-999999999999999999999999999999", 60 }
    };
    FILE *log;
    char line[256];
    unsigned logged = 0, named = 0;
    int failed = 0;

    CHECK(freopen(LOG_PATH, "w", stderr));
    /* Unset is silent. The first rejected value is logged once, with its text;
     * later rejections fall back silently. */
    CHECK(mm3_frame_limit("") == 60);
    CHECK(mm3_frame_limit("invalid") == 60);
    CHECK(mm3_frame_limit("91") == 91);
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        int actual = mm3_frame_limit(cases[i].value);
        if (actual != cases[i].expected) {
            printf("frame limit case %u: got %d, expected %d\n",
                   i, actual, cases[i].expected);
            failed = 1;
            break;
        }
    }
    fclose(stderr);

    log = fopen(LOG_PATH, "r");
    CHECK(log);
    while (fgets(line, sizeof line, log)) {
        if (strstr(line, "[FRAME_LIMIT]")) ++logged;
        if (strstr(line, "\"invalid\"")) ++named;
    }
    fclose(log);
    if (logged != 1 || named != 1) {
        printf("frame limit log: %u lines, %u naming the rejected value; expected 1 and 1\n",
               logged, named);
        failed = 1;
    }
    if (failed) return 1;
    puts("PASS: MM3 frame limit preserves lower rates and supports explicit unpaced mode and positive integer rates with safe invalid-value fallback, logged once");
    return 0;
}
