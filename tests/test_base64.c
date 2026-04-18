#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "../lib/aw-base64.h"

static void test_case(const char *input, const char *expected) {
    char out[128] = {0};
    size_t out_len = base64(out, sizeof(out), (const uint8_t *)input, strlen(input));
    out[out_len] = '\0';

    if (strcmp(out, expected) != 0) {
        printf("FAIL: base64(\"%s\")\n", input);
        printf("  expected: %s\n", expected);
        printf("  got:      %s\n", out);
        assert(0);
    }
}

int main(void) {
    printf("Running Base64 tests...\n");

    test_case("", "");
    test_case("f", "Zg==");
    test_case("fo", "Zm8=");
    test_case("foo", "Zm9v");
    test_case("foobar", "Zm9vYmFy");

    printf("All Base64 tests passed.\n");
    return 0;
}
