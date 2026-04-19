#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/aw-base64.h"

static void test_case(const char *input, const char *expected) {
  char out[128];

  /* Fill with '!' to detect missing null termination */
  memset(out, '!', sizeof(out));

  size_t out_len =
      base64(out, sizeof(out), (const uint8_t *)input, strlen(input));

  /* 1. Returned length must match expected */
  assert(out_len == strlen(expected));

  /* 2. Output must match expected string */
  assert(strcmp(out, expected) == 0);

  /* 3. Function must null‑terminate */
  assert(out[out_len] == '\0');
}

static void test_buffer_too_small(void) {
  char small_out[5]; /* Too small for "foobar" (needs 8 + 1 bytes) */

  size_t result =
      base64(small_out, sizeof(small_out), (const uint8_t *)"foobar", 6);

  /* Must return 0 and avoid out‑of‑bounds writes */
  assert(result == 0);
}

int main(void) {
  printf("Running Base64 tests...\n");

  /* RFC 4648 Test Vectors */
  test_case("", "");
  test_case("f", "Zg==");
  test_case("fo", "Zm8=");
  test_case("foo", "Zm9v");
  test_case("foob", "Zm9vYg==");
  test_case("fooba", "Zm9vYmE=");
  test_case("foobar", "Zm9vYmFy");

  /* Safety tests */
  test_buffer_too_small();

  printf("All Base64 tests passed.\n");
  return 0;
}
