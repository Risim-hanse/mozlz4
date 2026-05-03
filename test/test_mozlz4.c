/*
 * test_mozlz4.c — Test suite for mozlz4 format
 *
 * Build: make test
 * Run:   ./test_mozlz4
 */

#include "mozlz4.h"
#include "lz4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) do { \
    tests_run++; \
    printf("  [%02d] %-60s ", tests_run, name); \
    fflush(stdout); \
} while(0)

#define TEST_PASS() do { tests_passed++; printf("PASS\n"); } while(0)

#define TEST_FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { TEST_FAIL(msg); return; } \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { TEST_FAIL(msg); return; } \
} while(0)

#define ASSERT_MEM_EQ(a, b, len, msg) do { \
    if (memcmp((a), (b), (len)) != 0) { TEST_FAIL(msg); return; } \
} while(0)

static const uint8_t RUST_MAGIC[] = {'m', 'o', 'z', 'L', 'z', '4', '0', '\0'};

static void put_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

__attribute__((unused))
static uint8_t *build_mozlz4(const uint8_t *data, size_t data_len,
                             size_t *out_total_len)
{
    int cbound = LZ4_compressBound((int)data_len);
    size_t total = MOZLZ4_HEADER_LEN + (size_t)cbound;
    uint8_t *buf = (uint8_t *)malloc(total);
    int compressed;
    if (!buf) return NULL;

    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, (uint32_t)data_len);

    compressed = LZ4_compress_default(
        (const char *)data, (char *)(buf + MOZLZ4_HEADER_LEN),
        (int)data_len, cbound);
    if (compressed <= 0) { free(buf); return NULL; }

    *out_total_len = MOZLZ4_HEADER_LEN + (size_t)compressed;
    return buf;
}

static int roundtrip(const uint8_t *data, size_t data_len)
{
    size_t cbound = mozlz4_compress_bound(data_len);
    uint8_t *compressed = (uint8_t *)malloc(cbound);
    uint8_t *decompressed = (uint8_t *)malloc(data_len + 64);
    size_t comp_len = 0, decomp_len = 0;
    int rc;

    if (!compressed || !decompressed) {
        free(compressed);
        free(decompressed);
        return -1;
    }

    rc = mozlz4_compress(data, data_len, compressed, &comp_len, cbound);
    if (rc != MOZLZ4_OK) { free(compressed); free(decompressed); return -1; }

    rc = mozlz4_decompress(compressed, comp_len, decompressed, &decomp_len, data_len + 64);
    if (rc != MOZLZ4_OK) { free(compressed); free(decompressed); return -1; }

    if (decomp_len != data_len || memcmp(data, decompressed, data_len) != 0) {
        free(compressed);
        free(decompressed);
        return -1;
    }

    free(compressed);
    free(decompressed);
    return 0;
}


/* === Group 1: Magic Number === */

static void test_magic_exact_match(void)
{
    TEST_START("Magic number is exactly 'mozLz40\\0' (8 bytes)");
    ASSERT_EQ(sizeof(RUST_MAGIC), (size_t)8, "magic should be 8 bytes");
    ASSERT_EQ(RUST_MAGIC[0], 'm', "byte 0");
    ASSERT_EQ(RUST_MAGIC[7], '\0', "byte 7 null terminator");
    TEST_PASS();
}

static void test_magic_reject_wrong_prefix(void)
{
    TEST_START("Reject wrong magic number");
    uint8_t bad[] = {'m','o','z','L','z','4','1','\0', 0,0,0,0, 0};
    size_t out_len;
    uint8_t out[64];
    int rc = mozlz4_decompress(bad, sizeof(bad), out, &out_len, sizeof(out));
    ASSERT_EQ(rc, MOZLZ4_ERR_MAGIC, "should reject wrong magic");
    TEST_PASS();
}

static void test_magic_reject_lowercase(void)
{
    TEST_START("Reject lowercase variant 'mozlz40\\0'");
    uint8_t bad[] = {'m','o','z','l','z','4','0','\0', 0,0,0,0};
    size_t out_len;
    uint8_t out[64];
    int rc = mozlz4_decompress(bad, sizeof(bad), out, &out_len, sizeof(out));
    ASSERT_EQ(rc, MOZLZ4_ERR_MAGIC, "should reject lowercase");
    TEST_PASS();
}

static void test_magic_reject_no_null(void)
{
    TEST_START("Reject 'mozLz40X' (no null terminator)");
    uint8_t bad[] = {'m','o','z','L','z','4','0','X', 0,0,0,0};
    size_t out_len;
    uint8_t out[64];
    int rc = mozlz4_decompress(bad, sizeof(bad), out, &out_len, sizeof(out));
    ASSERT_EQ(rc, MOZLZ4_ERR_MAGIC, "should reject non-null terminator");
    TEST_PASS();
}

static void test_magic_reject_empty(void)
{
    TEST_START("Reject empty input");
    size_t out_len;
    uint8_t out[64];
    int rc = mozlz4_decompress((const uint8_t *)"", 0, out, &out_len, sizeof(out));
    ASSERT_TRUE(rc != MOZLZ4_OK, "should reject empty input");
    TEST_PASS();
}

static void test_magic_reject_short_input(void)
{
    TEST_START("Reject input shorter than 12 bytes (header)");
    uint8_t short_input[] = {'m','o','z','L','z','4','0','\0', 0,0,0};
    size_t out_len;
    uint8_t out[64];
    int rc = mozlz4_decompress(short_input, sizeof(short_input), out, &out_len, sizeof(out));
    ASSERT_TRUE(rc != MOZLZ4_OK, "should reject short input");
    TEST_PASS();
}

static void test_magic_header_exact_12(void)
{
    TEST_START("Accept 12-byte header-only input (decompressed_size=0)");
    uint8_t header[] = {'m','o','z','L','z','4','0','\0', 0,0,0,0};
    size_t out_len = 99;
    uint8_t out[64];
    int rc = mozlz4_decompress(header, sizeof(header), out, &out_len, sizeof(out));
    ASSERT_EQ(rc, MOZLZ4_OK, "12-byte header should succeed for size=0");
    ASSERT_EQ(out_len, (size_t)0, "decompressed length should be 0");
    TEST_PASS();
}


/* === Group 2: Size Field === */

static void test_size_field_le_encoding(void)
{
    TEST_START("Size field is little-endian uint32 at offset 8");
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 0x04030201);
    ASSERT_EQ(buf[8],  0x01, "LE byte 0");
    ASSERT_EQ(buf[9],  0x02, "LE byte 1");
    ASSERT_EQ(buf[10], 0x03, "LE byte 2");
    ASSERT_EQ(buf[11], 0x04, "LE byte 3");
    TEST_PASS();
}

static void test_size_field_read(void)
{
    TEST_START("mozlz4_read_size reads LE size correctly");
    uint8_t buf[MOZLZ4_HEADER_LEN];
    int ok = 0;
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 12345);
    uint32_t sz = mozlz4_read_size(buf, sizeof(buf), &ok);
    ASSERT_EQ(ok, 1, "ok flag");
    ASSERT_EQ(sz, (uint32_t)12345, "size value");
    TEST_PASS();
}

static void test_size_field_bad_magic(void)
{
    TEST_START("mozlz4_read_size returns 0 and ok=0 on bad magic");
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memset(buf, 0, sizeof(buf));
    int ok = 1;
    uint32_t sz = mozlz4_read_size(buf, sizeof(buf), &ok);
    ASSERT_EQ(ok, 0, "ok flag should be 0");
    ASSERT_EQ(sz, (uint32_t)0, "size should be 0");
    TEST_PASS();
}

static void test_size_field_max_value(void)
{
    TEST_START("Size field handles max uint32 (0xFFFFFFFF)");
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 0xFFFFFFFF);
    int ok = 0;
    uint32_t sz = mozlz4_read_size(buf, sizeof(buf), &ok);
    ASSERT_EQ(ok, 1, "ok flag");
    ASSERT_EQ(sz, (uint32_t)0xFFFFFFFF, "max uint32");
    TEST_PASS();
}


/* === Group 3: Roundtrip === */

static void test_roundtrip_empty(void)
{
    TEST_START("Roundtrip: empty data (0 bytes)");
    ASSERT_EQ(roundtrip((const uint8_t *)"", 0), 0, "empty roundtrip");
    TEST_PASS();
}

static void test_roundtrip_single_byte(void)
{
    TEST_START("Roundtrip: single byte");
    uint8_t data[] = {0x42};
    ASSERT_EQ(roundtrip(data, 1), 0, "single byte roundtrip");
    TEST_PASS();
}

static void test_roundtrip_hello(void)
{
    TEST_START("Roundtrip: 'Hello, World!'");
    const uint8_t *data = (const uint8_t *)"Hello, World!";
    ASSERT_EQ(roundtrip(data, 13), 0, "hello roundtrip");
    TEST_PASS();
}

static void test_roundtrip_small_json(void)
{
    TEST_START("Roundtrip: small JSON (simulating search.json)");
    const char *json = "{\"engines\":[{\"name\":\"Google\",\"url\":\"https://google.com/search?q={searchTerms}\"}]}";
    ASSERT_EQ(roundtrip((const uint8_t *)json, strlen(json)), 0, "json roundtrip");
    TEST_PASS();
}

static void test_roundtrip_1kb_repetitive(void)
{
    TEST_START("Roundtrip: 1KB repetitive data (highly compressible)");
    uint8_t data[1024];
    memset(data, 'A', sizeof(data));
    ASSERT_EQ(roundtrip(data, sizeof(data)), 0, "1kb repetitive roundtrip");
    TEST_PASS();
}

static void test_roundtrip_1kb_random(void)
{
    TEST_START("Roundtrip: 1KB pseudo-random data");
    uint8_t data[1024];
    uint32_t state = 12345;
    for (size_t i = 0; i < sizeof(data); i++) {
        state = state * 1103515245 + 12345;
        data[i] = (uint8_t)(state >> 16);
    }
    ASSERT_EQ(roundtrip(data, sizeof(data)), 0, "1kb random roundtrip");
    TEST_PASS();
}

static void test_roundtrip_4kb_json_like(void)
{
    TEST_START("Roundtrip: 4KB JSON-like data");
    char data[4096];
    int pos = 0;
    for (int i = 0; i < 50 && pos < (int)sizeof(data) - 100; i++) {
        pos += snprintf(data + pos, sizeof(data) - (size_t)pos,
            "{\"id\":%d,\"name\":\"engine_%d\",\"url\":\"https://example.com/search?q=test_%d\"},",
            i, i, i);
    }
    data[pos] = '\0';
    ASSERT_EQ(roundtrip((const uint8_t *)data, (size_t)pos), 0, "4kb json roundtrip");
    TEST_PASS();
}

static void test_roundtrip_64kb(void)
{
    TEST_START("Roundtrip: 64KB data");
    size_t sz = 65536;
    uint8_t *data = (uint8_t *)malloc(sz);
    ASSERT_TRUE(data != NULL, "alloc failed");
    for (size_t i = 0; i < sz; i++)
        data[i] = (uint8_t)(i & 0xFF);
    ASSERT_EQ(roundtrip(data, sz), 0, "64kb roundtrip");
    free(data);
    TEST_PASS();
}

static void test_roundtrip_all_zeros(void)
{
    TEST_START("Roundtrip: 256 bytes of zeros");
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    ASSERT_EQ(roundtrip(data, sizeof(data)), 0, "zeros roundtrip");
    TEST_PASS();
}

static void test_roundtrip_all_0xff(void)
{
    TEST_START("Roundtrip: 256 bytes of 0xFF");
    uint8_t data[256];
    memset(data, 0xFF, sizeof(data));
    ASSERT_EQ(roundtrip(data, sizeof(data)), 0, "0xff roundtrip");
    TEST_PASS();
}

static void test_roundtrip_binary_pattern(void)
{
    TEST_START("Roundtrip: binary pattern data");
    uint8_t data[512];
    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)((i * 7 + 13) ^ (i >> 3));
    ASSERT_EQ(roundtrip(data, sizeof(data)), 0, "binary pattern roundtrip");
    TEST_PASS();
}


/* === Group 4: Compression Format === */

static void test_compress_header_layout(void)
{
    TEST_START("Compressed output has correct header layout");
    const char *input = "test data for header layout check";
    size_t in_len = strlen(input);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *out = (uint8_t *)malloc(cbound);
    size_t out_len = 0;
    ASSERT_TRUE(out != NULL, "alloc");

    int rc = mozlz4_compress((const uint8_t *)input, in_len, out, &out_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress ok");
    ASSERT_MEM_EQ(out, RUST_MAGIC, MOZLZ4_MAGIC_LEN, "magic bytes");

    uint32_t stored_size = (uint32_t)out[8] | ((uint32_t)out[9] << 8) |
                           ((uint32_t)out[10] << 16) | ((uint32_t)out[11] << 24);
    ASSERT_EQ(stored_size, (uint32_t)in_len, "stored decompressed size");
    ASSERT_TRUE(out_len > MOZLZ4_HEADER_LEN, "output has compressed data");

    free(out);
    TEST_PASS();
}

static void test_compress_outlen_consistency(void)
{
    TEST_START("Compressed size is consistent with header + data");
    const char *input = "consistency check data";
    size_t in_len = strlen(input);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *out = (uint8_t *)malloc(cbound);
    size_t out_len = 0;
    ASSERT_TRUE(out != NULL, "alloc");

    int rc = mozlz4_compress((const uint8_t *)input, in_len, out, &out_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress ok");

    uint8_t *decomp = (uint8_t *)malloc(in_len + 64);
    size_t decomp_len = 0;
    ASSERT_TRUE(decomp != NULL, "alloc decomp");
    rc = mozlz4_decompress(out, out_len, decomp, &decomp_len, in_len + 64);
    ASSERT_EQ(rc, MOZLZ4_OK, "decompress ok");
    ASSERT_EQ(decomp_len, in_len, "decompressed size matches original");

    free(out);
    free(decomp);
    TEST_PASS();
}

static void test_compress_bound_value(void)
{
    TEST_START("mozlz4_compress_bound >= header + LZ4_compressBound");
    for (size_t sz = 0; sz <= 10000; sz = sz < 100 ? sz + 1 : sz * 3 + 1) {
        size_t mozlz4_bound = mozlz4_compress_bound(sz);
        int lz4_bound = LZ4_compressBound((int)sz);
        size_t expected_min = MOZLZ4_HEADER_LEN + (size_t)(lz4_bound > 0 ? lz4_bound : 0);
        if (mozlz4_bound < expected_min) {
            char msg[128];
            snprintf(msg, sizeof(msg), "bound too small for size %zu", sz);
            TEST_FAIL(msg);
            return;
        }
    }
    TEST_PASS();
}


/* === Group 5: Decompression Behavior === */

static void test_decompress_exact_match(void)
{
    TEST_START("Decompression produces exact byte-for-byte match");
    const char *original = "The quick brown fox jumps over the lazy dog. 0123456789";
    size_t in_len = strlen(original);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *compressed = (uint8_t *)malloc(cbound);
    uint8_t *decompressed = (uint8_t *)malloc(in_len + 64);
    size_t comp_len, decomp_len;
    ASSERT_TRUE(compressed && decompressed, "alloc");

    int rc = mozlz4_compress((const uint8_t *)original, in_len, compressed, &comp_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress");
    rc = mozlz4_decompress(compressed, comp_len, decompressed, &decomp_len, in_len + 64);
    ASSERT_EQ(rc, MOZLZ4_OK, "decompress");
    ASSERT_EQ(decomp_len, in_len, "length match");
    ASSERT_MEM_EQ(original, decompressed, in_len, "content match");

    free(compressed);
    free(decompressed);
    TEST_PASS();
}

static void test_decompress_safe_not_fast(void)
{
    TEST_START("Uses LZ4_decompress_safe — rejects undersized output");
    const char *original = "test data for safety check with enough content to compress";
    size_t in_len = strlen(original);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *compressed = (uint8_t *)malloc(cbound);
    size_t comp_len;
    ASSERT_TRUE(compressed, "alloc");

    int rc = mozlz4_compress((const uint8_t *)original, in_len, compressed, &comp_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress");

    uint8_t tiny_out[10];
    size_t decomp_len = 0;
    rc = mozlz4_decompress(compressed, comp_len, tiny_out, &decomp_len, sizeof(tiny_out));
    ASSERT_EQ(rc, MOZLZ4_ERR_DECOMPRESS, "should fail with undersized output");

    free(compressed);
    TEST_PASS();
}

static void test_decompress_trailing_zeros(void)
{
    TEST_START("Decompress with trailing zeros after compressed data");
    const char *input = "trailing bytes test";
    size_t in_len = strlen(input);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *compressed = (uint8_t *)malloc(cbound + 100);
    size_t comp_len = 0;
    ASSERT_TRUE(compressed, "alloc");

    int rc = mozlz4_compress((const uint8_t *)input, in_len, compressed, &comp_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress");

    memset(compressed + comp_len, 0x00, 100);
    uint8_t decompressed[256];
    size_t decomp_len = 0;
    rc = mozlz4_decompress(compressed, comp_len + 100, decompressed, &decomp_len, sizeof(decompressed));
    ASSERT_TRUE(rc == MOZLZ4_OK || rc == MOZLZ4_ERR_DECOMPRESS,
                "succeed or fail gracefully");

    decomp_len = 0;
    rc = mozlz4_decompress(compressed, comp_len, decompressed, &decomp_len, sizeof(decompressed));
    ASSERT_EQ(rc, MOZLZ4_OK, "exact length should succeed");
    ASSERT_EQ(decomp_len, in_len, "length correct");
    ASSERT_MEM_EQ(input, decompressed, in_len, "content correct");

    free(compressed);
    TEST_PASS();
}


/* === Group 6: Edge Cases === */

static void test_header_only_zero_size(void)
{
    TEST_START("Header-only file with size=0 decompresses to empty");
    uint8_t header[MOZLZ4_HEADER_LEN];
    memcpy(header, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(header + 8, 0);
    uint8_t out[1];
    size_t out_len = 99;
    int rc = mozlz4_decompress(header, sizeof(header), out, &out_len, sizeof(out));
    ASSERT_EQ(rc, MOZLZ4_OK, "should succeed");
    ASSERT_EQ(out_len, (size_t)0, "should produce 0 bytes");
    TEST_PASS();
}

static void test_compress_empty_input(void)
{
    TEST_START("Compress empty input");
    size_t cbound = mozlz4_compress_bound(0);
    uint8_t *out = (uint8_t *)malloc(cbound);
    size_t out_len = 0;
    ASSERT_TRUE(out, "alloc");

    int rc = mozlz4_compress((const uint8_t *)"", 0, out, &out_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress empty");
    ASSERT_TRUE(out_len >= MOZLZ4_HEADER_LEN, "at least header size");

    ASSERT_MEM_EQ(out, RUST_MAGIC, MOZLZ4_MAGIC_LEN, "magic");
    uint32_t sz = (uint32_t)out[8] | ((uint32_t)out[9] << 8) |
                  ((uint32_t)out[10] << 16) | ((uint32_t)out[11] << 24);
    ASSERT_EQ(sz, (uint32_t)0, "decompressed size should be 0");

    free(out);
    TEST_PASS();
}

static void test_compress_tiny_buffer(void)
{
    TEST_START("Compress with exact-fit output buffer");
    uint8_t data[] = {0xAA};
    size_t cbound = mozlz4_compress_bound(1);
    uint8_t *out = (uint8_t *)malloc(cbound);
    size_t out_len = 0;
    ASSERT_TRUE(out, "alloc");

    int rc = mozlz4_compress(data, 1, out, &out_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress with exact buffer");

    free(out);
    TEST_PASS();
}

static void test_size_field_equals_original(void)
{
    TEST_START("Stored decompressed size equals original input length");
    size_t sizes[] = {0, 1, 100, 1000, 65536};
    for (int i = 0; i < 5; i++) {
        size_t sz = sizes[i];
        uint8_t *data = (uint8_t *)malloc(sz > 0 ? sz : 1);
        if (sz > 0) memset(data, 'B', sz);

        size_t cbound = mozlz4_compress_bound(sz);
        uint8_t *out = (uint8_t *)malloc(cbound);
        size_t out_len = 0;

        if (data && out) {
            int rc = mozlz4_compress(data, sz, out, &out_len, cbound);
            if (rc == MOZLZ4_OK) {
                int ok = 0;
                uint32_t stored = mozlz4_read_size(out, out_len, &ok);
                if (!ok || stored != (uint32_t)sz) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "size mismatch for %zu: got %u", sz, stored);
                    free(data); free(out);
                    TEST_FAIL(msg);
                    return;
                }
            }
        }

        free(data);
        free(out);
    }
    TEST_PASS();
}

static void test_compressed_smaller_for_compressible(void)
{
    TEST_START("Compressed output is smaller for highly compressible data");
    uint8_t data[4096];
    memset(data, 'X', sizeof(data));

    size_t cbound = mozlz4_compress_bound(sizeof(data));
    uint8_t *compressed = (uint8_t *)malloc(cbound);
    size_t comp_len = 0;
    ASSERT_TRUE(compressed, "alloc");

    int rc = mozlz4_compress(data, sizeof(data), compressed, &comp_len, cbound);
    ASSERT_EQ(rc, MOZLZ4_OK, "compress");
    ASSERT_TRUE(comp_len < sizeof(data), "compressed smaller than original");

    free(compressed);
    TEST_PASS();
}

static void test_header_always_present(void)
{
    TEST_START("Compressed output always has at least MOZLZ4_HEADER_LEN bytes");
    for (size_t sz = 0; sz <= 1000; sz = sz < 10 ? sz + 1 : sz * 5) {
        uint8_t *data = (uint8_t *)malloc(sz > 0 ? sz : 1);
        if (sz > 0) memset(data, 'A', sz);

        size_t cbound = mozlz4_compress_bound(sz);
        uint8_t *out = (uint8_t *)malloc(cbound);
        size_t out_len = 0;

        if (data && out) {
            int rc = mozlz4_compress(data, sz, out, &out_len, cbound);
            if (rc == MOZLZ4_OK && out_len < MOZLZ4_HEADER_LEN) {
                char msg[64];
                snprintf(msg, sizeof(msg), "output too short for size %zu: %zu", sz, out_len);
                free(data); free(out);
                TEST_FAIL(msg);
                return;
            }
        }

        free(data);
        free(out);
    }
    TEST_PASS();
}

static void test_roundtrip_256kb(void)
{
    TEST_START("Roundtrip: 256KB data");
    size_t sz = 256 * 1024;
    uint8_t *data = (uint8_t *)malloc(sz);
    ASSERT_TRUE(data, "alloc");
    for (size_t i = 0; i < sz; i++)
        data[i] = (uint8_t)((i * 31 + 17) & 0xFF);
    ASSERT_EQ(roundtrip(data, sz), 0, "256kb roundtrip");
    free(data);
    TEST_PASS();
}

static void test_roundtrip_1mb(void)
{
    TEST_START("Roundtrip: 1MB data");
    size_t sz = 1024 * 1024;
    uint8_t *data = (uint8_t *)malloc(sz);
    ASSERT_TRUE(data, "alloc");
    for (size_t i = 0; i < sz; i++)
        data[i] = (uint8_t)((i >> 8) ^ i);
    ASSERT_EQ(roundtrip(data, sz), 0, "1mb roundtrip");
    free(data);
    TEST_PASS();
}

static void test_every_byte_pattern(void)
{
    TEST_START("Every byte preserved through compress+decompress (256 patterns)");
    for (int pattern = 0; pattern < 256; pattern++) {
        uint8_t data[128];
        memset(data, (uint8_t)pattern, sizeof(data));
        if (roundtrip(data, sizeof(data)) != 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "failed for pattern 0x%02X", pattern);
            TEST_FAIL(msg);
            return;
        }
    }
    TEST_PASS();
}


/* === Main === */

int main(void)
{
    printf("\n=== mozlz4 Test Suite ===\n\n");

    printf("[Group 1] Magic Number\n");
    test_magic_exact_match();
    test_magic_reject_wrong_prefix();
    test_magic_reject_lowercase();
    test_magic_reject_no_null();
    test_magic_reject_empty();
    test_magic_reject_short_input();
    test_magic_header_exact_12();

    printf("\n[Group 2] Size Field\n");
    test_size_field_le_encoding();
    test_size_field_read();
    test_size_field_bad_magic();
    test_size_field_max_value();

    printf("\n[Group 3] Roundtrip (compress -> decompress)\n");
    test_roundtrip_empty();
    test_roundtrip_single_byte();
    test_roundtrip_hello();
    test_roundtrip_small_json();
    test_roundtrip_1kb_repetitive();
    test_roundtrip_1kb_random();
    test_roundtrip_4kb_json_like();
    test_roundtrip_64kb();
    test_roundtrip_all_zeros();
    test_roundtrip_all_0xff();
    test_roundtrip_binary_pattern();

    printf("\n[Group 4] Compression Format\n");
    test_compress_header_layout();
    test_compress_outlen_consistency();
    test_compress_bound_value();

    printf("\n[Group 5] Decompression Behavior\n");
    test_decompress_exact_match();
    test_decompress_safe_not_fast();
    test_decompress_trailing_zeros();

    printf("\n[Group 6] Edge Cases & Invariants\n");
    test_header_only_zero_size();
    test_compress_empty_input();
    test_compress_tiny_buffer();
    test_size_field_equals_original();
    test_compressed_smaller_for_compressible();
    test_header_always_present();
    test_roundtrip_256kb();
    test_roundtrip_1mb();
    test_every_byte_pattern();

    printf("\n=== Results ===\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}
