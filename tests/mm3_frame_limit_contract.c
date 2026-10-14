#include <stdio.h>
#include "mm3_frame_limit.h"

int main(void)
{
    static const struct { const char *value; int expected; } cases[] = {
        { NULL, 60 }, { "", 60 }, { "60", 60 }, { "59", 59 },
        { "30", 30 }, { "1", 1 }, { "0", 60 }, { "-1", 60 },
        { "61", 61 }, { "90", 90 }, { "91", 60 }, { "120", 60 }, { "165", 60 },
        { "invalid", 60 }, { "30junk", 60 }, { "59.94", 60 },
        { " 30\t ", 30 }, { "999999999999999999999999999999", 60 },
        { "-999999999999999999999999999999", 60 }
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        int actual = mm3_frame_limit(cases[i].value);
        if (actual != cases[i].expected) {
            fprintf(stderr, "frame limit case %u: got %d, expected %d\n",
                    i, actual, cases[i].expected);
            return 1;
        }
    }
    puts("PASS: MM3 frame limit preserves lower rates and supports configured rates through 90 with safe invalid-value fallback");
    return 0;
}
