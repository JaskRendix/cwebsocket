#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "../lib/aw-sha1.h"   // adjust path if needed

// Convert binary digest to hex string for readable comparison
static void to_hex(const unsigned char *in, char *out) {
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < SHA1_SIZE; i++) {
        out[i*2]     = hex[(in[i] >> 4) & 0xF];
        out[i*2 + 1] = hex[in[i] & 0xF];
    }
    out[SHA1_SIZE * 2] = '\0';
}

static void test_vector(const char *msg, const char *expected_hex) {
    unsigned char digest[SHA1_SIZE];
    char digest_hex[SHA1_SIZE * 2 + 1];

    sha1(digest, msg, strlen(msg));
    to_hex(digest, digest_hex);

    if (strcmp(digest_hex, expected_hex) != 0) {
        printf("FAIL: sha1(\"%s\")\n", msg);
        printf("  expected: %s\n", expected_hex);
        printf("  got:      %s\n", digest_hex);
        assert(0 && "SHA1 test vector mismatch");
    }
}

int main(void) {
    printf("Running SHA1 tests...\n");

    // Standard test vectors from RFC 3174

    test_vector("",
        "da39a3ee5e6b4b0d3255bfef95601890afd80709");

    test_vector("abc",
        "a9993e364706816aba3e25717850c26c9cd0d89d");

    test_vector("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

    test_vector("The quick brown fox jumps over the lazy dog",
        "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

    test_vector("The quick brown fox jumps over the lazy cog",
        "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3");

    printf("All SHA1 tests passed.\n");
    return 0;
}
