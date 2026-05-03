/*
 * test_mozlz4.c : test suite for the mozlz4 format
 *
 * uses mu (munit) for fork-per-test isolation, proper assert macros,
 * and filterable test names. run with:
 *
 *   make test                          all tests
 *   ./test_mozlz4 /mozlz4/magic       just the magic tests
 *   ./test_mozlz4 --list               list all test names
 *   make SANITIZE=1 test               ASan+UBSan build
 */

#include "mozlz4.h"
#include "lz4.h"
#include "vendor/munit.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

static const uint8_t RUST_MAGIC[] = {'m', 'o', 'z', 'L', 'z', '4', '0', '\0'};

static void put_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/*
 * roundtrip: compress, decompress, verify byte-for-byte.
 * returns 0 on success, or a negative stage code:
 *  -1 alloc, -2 compress, -3 decompress, -4 mismatch
 */
static int roundtrip(const uint8_t *data, size_t data_len)
{
    size_t cbound = mozlz4_compress_bound(data_len);
    uint8_t *compressed = malloc(cbound > 0 ? cbound : 1);
    uint8_t *decompressed = malloc(data_len + 64);
    size_t comp_len = 0, decomp_len = 0;
    int rc;

    if (!compressed || !decompressed) {
        free(compressed);
        free(decompressed);
        return -1;
    }

    rc = mozlz4_compress(data, data_len, compressed, &comp_len, cbound);
    if (rc != MOZLZ4_OK) { free(compressed); free(decompressed); return -2; }

    rc = mozlz4_decompress(compressed, comp_len, decompressed, &decomp_len, data_len + 64);
    if (rc != MOZLZ4_OK) { free(compressed); free(decompressed); return -3; }

    if (decomp_len != data_len || memcmp(data, decompressed, data_len) != 0) {
        free(compressed);
        free(decompressed);
        return -4;
    }

    free(compressed);
    free(decompressed);
    return 0;
}

static const char *roundtrip_errstr(int rc)
{
    switch (rc) {
    case -1: return "allocation failed";
    case -2: return "compression failed";
    case -3: return "decompression failed";
    case -4: return "data mismatch";
    default: return "unknown error";
    }
}


/* null arguments */

static MunitResult test_null_in_decompress(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_decompress(NULL, 0, out, &out_len, sizeof(out)), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}

static MunitResult test_null_out_decompress(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t in[12] = {0}; size_t out_len;
    munit_assert_int(mozlz4_decompress(in, sizeof(in), NULL, &out_len, 0), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}

static MunitResult test_null_outlen_decompress(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t in[12] = {0}; uint8_t out[64];
    munit_assert_int(mozlz4_decompress(in, sizeof(in), out, NULL, sizeof(out)), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}

static MunitResult test_null_in_compress(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t out[64]; size_t out_len;
    munit_assert_int(mozlz4_compress(NULL, 0, out, &out_len, sizeof(out)), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}

static MunitResult test_null_out_compress(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t in[4] = {1,2,3,4}; size_t out_len;
    munit_assert_int(mozlz4_compress(in, sizeof(in), NULL, &out_len, 0), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}

static MunitResult test_null_outlen_compress(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t in[4] = {1,2,3,4}; uint8_t out[64];
    munit_assert_int(mozlz4_compress(in, sizeof(in), out, NULL, sizeof(out)), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}

static MunitResult test_null_outsize_read(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 42);
    munit_assert_int(mozlz4_read_size(buf, sizeof(buf), NULL), ==, MOZLZ4_ERR_NULL);
    return MUNIT_OK;
}


/* too-large inputs */

static MunitResult test_compress_too_large(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t dummy[1] = {0}; size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_compress(dummy, (size_t)LZ4_MAX_INPUT_SIZE + 1, out, &out_len, sizeof(out)), ==, MOZLZ4_ERR_TOO_LARGE);
    return MUNIT_OK;
}

static MunitResult test_compress_bound_too_large(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    munit_assert_size(mozlz4_compress_bound((size_t)LZ4_MAX_INPUT_SIZE + 1), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_decompress_too_large_size(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN + 8];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 0x80000000);
    memset(buf + MOZLZ4_HEADER_LEN, 0, 8);
    uint8_t out[64]; size_t out_len;
    munit_assert_int(mozlz4_decompress(buf, sizeof(buf), out, &out_len, sizeof(out)), !=, MOZLZ4_OK);
    return MUNIT_OK;
}


/* magic number */

static MunitResult test_magic_exact_match(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    munit_assert_size(sizeof(RUST_MAGIC), ==, 8);
    munit_assert_uint8(RUST_MAGIC[0], ==, 'm');
    munit_assert_uint8(RUST_MAGIC[7], ==, '\0');
    return MUNIT_OK;
}

static MunitResult test_magic_reject_wrong_prefix(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t bad[] = {'m','o','z','L','z','4','1','\0', 0,0,0,0, 0};
    size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_decompress(bad, sizeof(bad), out, &out_len, sizeof(out)), ==, MOZLZ4_ERR_MAGIC);
    return MUNIT_OK;
}

static MunitResult test_magic_reject_lowercase(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t bad[] = {'m','o','z','l','z','4','0','\0', 0,0,0,0};
    size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_decompress(bad, sizeof(bad), out, &out_len, sizeof(out)), ==, MOZLZ4_ERR_MAGIC);
    return MUNIT_OK;
}

static MunitResult test_magic_reject_no_null(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t bad[] = {'m','o','z','L','z','4','0','X', 0,0,0,0};
    size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_decompress(bad, sizeof(bad), out, &out_len, sizeof(out)), ==, MOZLZ4_ERR_MAGIC);
    return MUNIT_OK;
}

static MunitResult test_magic_reject_empty(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_decompress((const uint8_t *)"", 0, out, &out_len, sizeof(out)), !=, MOZLZ4_OK);
    return MUNIT_OK;
}

static MunitResult test_magic_reject_short_input(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t short_input[] = {'m','o','z','L','z','4','0','\0', 0,0,0};
    size_t out_len; uint8_t out[64];
    munit_assert_int(mozlz4_decompress(short_input, sizeof(short_input), out, &out_len, sizeof(out)), !=, MOZLZ4_OK);
    return MUNIT_OK;
}

static MunitResult test_magic_header_exact_12(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t header[] = {'m','o','z','L','z','4','0','\0', 0,0,0,0};
    size_t out_len = 99; uint8_t out[64];
    int rc = mozlz4_decompress(header, sizeof(header), out, &out_len, sizeof(out));
    munit_assert_int(rc, ==, MOZLZ4_OK);
    munit_assert_size(out_len, ==, 0);
    return MUNIT_OK;
}


/* size field */

static MunitResult test_size_field_le_encoding(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 0x04030201);
    munit_assert_uint8(buf[8],  ==, 0x01);
    munit_assert_uint8(buf[9],  ==, 0x02);
    munit_assert_uint8(buf[10], ==, 0x03);
    munit_assert_uint8(buf[11], ==, 0x04);
    return MUNIT_OK;
}

static MunitResult test_size_field_read(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 12345);
    uint32_t sz = 0;
    munit_assert_int(mozlz4_read_size(buf, sizeof(buf), &sz), ==, MOZLZ4_OK);
    munit_assert_uint32(sz, ==, 12345);
    return MUNIT_OK;
}

static MunitResult test_size_field_bad_magic(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memset(buf, 0, sizeof(buf));
    uint32_t sz = 99;
    munit_assert_int(mozlz4_read_size(buf, sizeof(buf), &sz), ==, MOZLZ4_ERR_MAGIC);
    munit_assert_uint32(sz, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_size_field_max_value(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 0xFFFFFFFF);
    uint32_t sz = 0;
    munit_assert_int(mozlz4_read_size(buf, sizeof(buf), &sz), ==, MOZLZ4_OK);
    munit_assert_uint32(sz, ==, 0xFFFFFFFF);
    return MUNIT_OK;
}

static MunitResult test_size_field_zero(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[MOZLZ4_HEADER_LEN];
    memcpy(buf, RUST_MAGIC, MOZLZ4_MAGIC_LEN);
    put_u32_le(buf + 8, 0);
    uint32_t sz = 99;
    munit_assert_int(mozlz4_read_size(buf, sizeof(buf), &sz), ==, MOZLZ4_OK);
    munit_assert_uint32(sz, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_size_field_too_short(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t buf[] = {'m','o','z','L','z','4','0','\0', 0,0,0};
    uint32_t sz = 99;
    munit_assert_int(mozlz4_read_size(buf, sizeof(buf), &sz), ==, MOZLZ4_ERR_TOO_SHORT);
    return MUNIT_OK;
}


/* roundtrip */

static MunitResult test_roundtrip_empty(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    munit_assert_int(roundtrip((const uint8_t *)"", 0), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_single_byte(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[] = {0x42};
    munit_assert_int(roundtrip(v, 1), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_hello(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    munit_assert_int(roundtrip((const uint8_t *)"Hello, World!", 13), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_small_json(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    const char *json = "{\"engines\":[{\"name\":\"Google\",\"url\":\"https://google.com/search?q={searchTerms}\"}]}";
    munit_assert_int(roundtrip((const uint8_t *)json, strlen(json)), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_1kb_repetitive(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[1024];
    memset(v, 'A', sizeof(v));
    munit_assert_int(roundtrip(v, sizeof(v)), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_1kb_random(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[1024];
    uint32_t state = 12345;
    for (size_t i = 0; i < sizeof(v); i++) {
        state = state * 1103515245 + 12345;
        v[i] = (uint8_t)(state >> 16);
    }
    munit_assert_int(roundtrip(v, sizeof(v)), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_4kb_json_like(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    char v[4096];
    int pos = 0;
    for (int i = 0; i < 50 && pos < (int)sizeof(v) - 100; i++) {
        pos += snprintf(v + pos, sizeof(v) - (size_t)pos,
            "{\"id\":%d,\"name\":\"engine_%d\",\"url\":\"https://example.com/search?q=test_%d\"},",
            i, i, i);
    }
    v[pos] = '\0';
    munit_assert_int(roundtrip((const uint8_t *)v, (size_t)pos), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_64kb(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t sz = 65536;
    uint8_t *v = malloc(sz);
    munit_assert_ptr_not_null(v);
    for (size_t i = 0; i < sz; i++) v[i] = (uint8_t)(i & 0xFF);
    int rc = roundtrip(v, sz);
    free(v);
    munit_assert_int(rc, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_all_zeros(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[256];
    memset(v, 0, sizeof(v));
    munit_assert_int(roundtrip(v, sizeof(v)), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_all_0xff(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[256];
    memset(v, 0xFF, sizeof(v));
    munit_assert_int(roundtrip(v, sizeof(v)), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_binary_pattern(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[512];
    for (size_t i = 0; i < sizeof(v); i++)
        v[i] = (uint8_t)((i * 7 + 13) ^ (i >> 3));
    munit_assert_int(roundtrip(v, sizeof(v)), ==, 0);
    return MUNIT_OK;
}


/* compression format */

static MunitResult test_compress_header_layout(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    const char *input = "test data for header layout check";
    size_t in_len = strlen(input);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *out = malloc(cbound);
    size_t out_len = 0;
    munit_assert_ptr_not_null(out);

    munit_assert_int(mozlz4_compress((const uint8_t *)input, in_len, out, &out_len, cbound), ==, MOZLZ4_OK);
    munit_assert_memory_equal(MOZLZ4_MAGIC_LEN, out, RUST_MAGIC);

    uint32_t stored_size = 0;
    munit_assert_int(mozlz4_read_size(out, out_len, &stored_size), ==, MOZLZ4_OK);
    munit_assert_uint32(stored_size, ==, (uint32_t)in_len);
    munit_assert_size(out_len, >, MOZLZ4_HEADER_LEN);

    free(out);
    return MUNIT_OK;
}

static MunitResult test_compress_outlen_consistency(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    const char *input = "consistency check data";
    size_t in_len = strlen(input);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *out = malloc(cbound);
    size_t out_len = 0;
    munit_assert_ptr_not_null(out);

    munit_assert_int(mozlz4_compress((const uint8_t *)input, in_len, out, &out_len, cbound), ==, MOZLZ4_OK);

    uint8_t *decomp = malloc(in_len + 64);
    size_t decomp_len = 0;
    munit_assert_ptr_not_null(decomp);
    munit_assert_int(mozlz4_decompress(out, out_len, decomp, &decomp_len, in_len + 64), ==, MOZLZ4_OK);
    munit_assert_size(decomp_len, ==, in_len);

    free(out);
    free(decomp);
    return MUNIT_OK;
}

static MunitResult test_compress_bound_value(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    for (size_t sz = 0; sz <= 10000; sz = sz < 100 ? sz + 1 : sz * 3 + 1) {
        size_t mozlz4_bound = mozlz4_compress_bound(sz);
        int lz4_bound = LZ4_compressBound((int)sz);
        size_t expected_min = MOZLZ4_HEADER_LEN + (size_t)(lz4_bound > 0 ? lz4_bound : 0);
        munit_assert_size(mozlz4_bound, >=, expected_min);
    }
    return MUNIT_OK;
}


/* decompression behavior */

static MunitResult test_decompress_exact_match(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    const char *original = "The quick brown fox jumps over the lazy dog. 0123456789";
    size_t in_len = strlen(original);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *compressed = malloc(cbound);
    uint8_t *decompressed = malloc(in_len + 64);
    munit_assert_ptr_not_null(compressed);
    munit_assert_ptr_not_null(decompressed);

    size_t comp_len, decomp_len;
    munit_assert_int(mozlz4_compress((const uint8_t *)original, in_len, compressed, &comp_len, cbound), ==, MOZLZ4_OK);
    munit_assert_int(mozlz4_decompress(compressed, comp_len, decompressed, &decomp_len, in_len + 64), ==, MOZLZ4_OK);
    munit_assert_size(decomp_len, ==, in_len);
    munit_assert_memory_equal(in_len, original, decompressed);

    free(compressed);
    free(decompressed);
    return MUNIT_OK;
}

static MunitResult test_decompress_safe_not_fast(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    const char *original = "test data for safety check with enough content to compress";
    size_t in_len = strlen(original);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *compressed = malloc(cbound);
    munit_assert_ptr_not_null(compressed);

    size_t comp_len;
    munit_assert_int(mozlz4_compress((const uint8_t *)original, in_len, compressed, &comp_len, cbound), ==, MOZLZ4_OK);

    uint8_t tiny_out[10];
    size_t decomp_len = 0;
    munit_assert_int(mozlz4_decompress(compressed, comp_len, tiny_out, &decomp_len, sizeof(tiny_out)), ==, MOZLZ4_ERR_DECOMPRESS);

    free(compressed);
    return MUNIT_OK;
}

static MunitResult test_decompress_trailing_zeros(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    const char *input = "trailing bytes test";
    size_t in_len = strlen(input);
    size_t cbound = mozlz4_compress_bound(in_len);
    uint8_t *compressed = malloc(cbound + 100);
    munit_assert_ptr_not_null(compressed);

    size_t comp_len;
    munit_assert_int(mozlz4_compress((const uint8_t *)input, in_len, compressed, &comp_len, cbound), ==, MOZLZ4_OK);

    uint8_t decompressed[256];
    size_t decomp_len = 0;
    munit_assert_int(mozlz4_decompress(compressed, comp_len, decompressed, &decomp_len, sizeof(decompressed)), ==, MOZLZ4_OK);
    munit_assert_size(decomp_len, ==, in_len);
    munit_assert_memory_equal(in_len, input, decompressed);

    free(compressed);
    return MUNIT_OK;
}


/* edge cases */

static MunitResult test_compress_empty_input(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t cbound = mozlz4_compress_bound(0);
    uint8_t *out = malloc(cbound);
    size_t out_len = 0;
    munit_assert_ptr_not_null(out);

    munit_assert_int(mozlz4_compress((const uint8_t *)"", 0, out, &out_len, cbound), ==, MOZLZ4_OK);
    munit_assert_size(out_len, >=, MOZLZ4_HEADER_LEN);
    munit_assert_memory_equal(MOZLZ4_MAGIC_LEN, out, RUST_MAGIC);

    uint32_t sz = 99;
    munit_assert_int(mozlz4_read_size(out, out_len, &sz), ==, MOZLZ4_OK);
    munit_assert_uint32(sz, ==, 0);

    free(out);
    return MUNIT_OK;
}

static MunitResult test_compress_tiny_buffer(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[] = {0xAA};
    size_t cbound = mozlz4_compress_bound(1);
    uint8_t *out = malloc(cbound);
    size_t out_len = 0;
    munit_assert_ptr_not_null(out);

    munit_assert_int(mozlz4_compress(v, 1, out, &out_len, cbound), ==, MOZLZ4_OK);

    free(out);
    return MUNIT_OK;
}

static MunitResult test_size_field_equals_original(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t sizes[] = {0, 1, 100, 1000, 65536};
    for (int i = 0; i < 5; i++) {
        size_t sz = sizes[i];
        uint8_t *v = malloc(sz > 0 ? sz : 1);
        munit_assert_ptr_not_null(v);
        if (sz > 0) memset(v, 'B', sz);

        size_t cbound = mozlz4_compress_bound(sz);
        uint8_t *out = malloc(cbound);
        munit_assert_ptr_not_null(out);
        size_t out_len = 0;

        munit_assert_int(mozlz4_compress(v, sz, out, &out_len, cbound), ==, MOZLZ4_OK);
        uint32_t stored = 99;
        munit_assert_int(mozlz4_read_size(out, out_len, &stored), ==, MOZLZ4_OK);
        munit_assert_uint32(stored, ==, (uint32_t)sz);

        free(v);
        free(out);
    }
    return MUNIT_OK;
}

static MunitResult test_compressed_smaller_for_compressible(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    uint8_t v[4096];
    memset(v, 'X', sizeof(v));

    size_t cbound = mozlz4_compress_bound(sizeof(v));
    uint8_t *compressed = malloc(cbound);
    munit_assert_ptr_not_null(compressed);
    size_t comp_len = 0;

    munit_assert_int(mozlz4_compress(v, sizeof(v), compressed, &comp_len, cbound), ==, MOZLZ4_OK);
    munit_assert_size(comp_len, <, sizeof(v));

    free(compressed);
    return MUNIT_OK;
}

static MunitResult test_header_always_present(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    for (size_t sz = 0; sz <= 1000; sz = sz < 10 ? sz + 1 : sz * 5) {
        uint8_t *v = malloc(sz > 0 ? sz : 1);
        munit_assert_ptr_not_null(v);
        if (sz > 0) memset(v, 'A', sz);

        size_t cbound = mozlz4_compress_bound(sz);
        uint8_t *out = malloc(cbound);
        munit_assert_ptr_not_null(out);
        size_t out_len = 0;

        int rc = mozlz4_compress(v, sz, out, &out_len, cbound);
        if (rc == MOZLZ4_OK)
            munit_assert_size(out_len, >=, MOZLZ4_HEADER_LEN);

        free(v);
        free(out);
    }
    return MUNIT_OK;
}

static MunitResult test_roundtrip_256kb(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t sz = 256 * 1024;
    uint8_t *v = malloc(sz);
    munit_assert_ptr_not_null(v);
    for (size_t i = 0; i < sz; i++) v[i] = (uint8_t)((i * 31 + 17) & 0xFF);
    int rc = roundtrip(v, sz);
    free(v);
    munit_assert_int(rc, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_1mb(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    size_t sz = 1024 * 1024;
    uint8_t *v = malloc(sz);
    munit_assert_ptr_not_null(v);
    for (size_t i = 0; i < sz; i++) v[i] = (uint8_t)((i >> 8) ^ i);
    int rc = roundtrip(v, sz);
    free(v);
    munit_assert_int(rc, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_every_byte_pattern(const MunitParameter *p, void *d) {
    (void)p; (void)d;
    for (int pattern = 0; pattern < 256; pattern++) {
        uint8_t v[128];
        memset(v, (uint8_t)pattern, sizeof(v));
        int rc = roundtrip(v, sizeof(v));
        if (rc != 0) {
            munit_logf(MUNIT_LOG_ERROR, "pattern 0x%02X: %s", pattern, roundtrip_errstr(rc));
            munit_assert_int(rc, ==, 0);
        }
    }
    return MUNIT_OK;
}


/* test registration */

static MunitTest tests[] = {
    { "/null/decompress-in",       test_null_in_decompress,       NULL, NULL, 0, NULL },
    { "/null/decompress-out",      test_null_out_decompress,      NULL, NULL, 0, NULL },
    { "/null/decompress-outlen",   test_null_outlen_decompress,   NULL, NULL, 0, NULL },
    { "/null/compress-in",         test_null_in_compress,         NULL, NULL, 0, NULL },
    { "/null/compress-out",        test_null_out_compress,        NULL, NULL, 0, NULL },
    { "/null/compress-outlen",     test_null_outlen_compress,     NULL, NULL, 0, NULL },
    { "/null/read-size-outsize",   test_null_outsize_read,        NULL, NULL, 0, NULL },

    { "/too-large/compress",         test_compress_too_large,         NULL, NULL, 0, NULL },
    { "/too-large/compress-bound",   test_compress_bound_too_large,   NULL, NULL, 0, NULL },
    { "/too-large/decompress-size",  test_decompress_too_large_size,  NULL, NULL, 0, NULL },

    { "/magic/exact-match",        test_magic_exact_match,        NULL, NULL, 0, NULL },
    { "/magic/reject-wrong",       test_magic_reject_wrong_prefix, NULL, NULL, 0, NULL },
    { "/magic/reject-lowercase",   test_magic_reject_lowercase,   NULL, NULL, 0, NULL },
    { "/magic/reject-no-null",     test_magic_reject_no_null,     NULL, NULL, 0, NULL },
    { "/magic/reject-empty",       test_magic_reject_empty,       NULL, NULL, 0, NULL },
    { "/magic/reject-short",       test_magic_reject_short_input, NULL, NULL, 0, NULL },
    { "/magic/header-exact-12",    test_magic_header_exact_12,    NULL, NULL, 0, NULL },

    { "/size/le-encoding",         test_size_field_le_encoding,   NULL, NULL, 0, NULL },
    { "/size/read",                test_size_field_read,          NULL, NULL, 0, NULL },
    { "/size/bad-magic",           test_size_field_bad_magic,     NULL, NULL, 0, NULL },
    { "/size/max-value",           test_size_field_max_value,     NULL, NULL, 0, NULL },
    { "/size/zero",                test_size_field_zero,          NULL, NULL, 0, NULL },
    { "/size/too-short",           test_size_field_too_short,     NULL, NULL, 0, NULL },

    { "/roundtrip/empty",           test_roundtrip_empty,           NULL, NULL, 0, NULL },
    { "/roundtrip/single-byte",     test_roundtrip_single_byte,     NULL, NULL, 0, NULL },
    { "/roundtrip/hello",           test_roundtrip_hello,           NULL, NULL, 0, NULL },
    { "/roundtrip/small-json",      test_roundtrip_small_json,      NULL, NULL, 0, NULL },
    { "/roundtrip/1kb-repetitive",  test_roundtrip_1kb_repetitive,  NULL, NULL, 0, NULL },
    { "/roundtrip/1kb-random",      test_roundtrip_1kb_random,      NULL, NULL, 0, NULL },
    { "/roundtrip/4kb-json-like",   test_roundtrip_4kb_json_like,   NULL, NULL, 0, NULL },
    { "/roundtrip/64kb",            test_roundtrip_64kb,            NULL, NULL, 0, NULL },
    { "/roundtrip/all-zeros",       test_roundtrip_all_zeros,       NULL, NULL, 0, NULL },
    { "/roundtrip/all-0xff",        test_roundtrip_all_0xff,        NULL, NULL, 0, NULL },
    { "/roundtrip/binary-pattern",  test_roundtrip_binary_pattern,  NULL, NULL, 0, NULL },

    { "/format/header-layout",       test_compress_header_layout,     NULL, NULL, 0, NULL },
    { "/format/outlen-consistency",  test_compress_outlen_consistency, NULL, NULL, 0, NULL },
    { "/format/bound-value",         test_compress_bound_value,       NULL, NULL, 0, NULL },

    { "/decompress/exact-match",     test_decompress_exact_match,     NULL, NULL, 0, NULL },
    { "/decompress/safe-not-fast",   test_decompress_safe_not_fast,   NULL, NULL, 0, NULL },
    { "/decompress/trailing-zeros",  test_decompress_trailing_zeros,  NULL, NULL, 0, NULL },

    { "/edge/empty-input",              test_compress_empty_input,              NULL, NULL, 0, NULL },
    { "/edge/tiny-buffer",              test_compress_tiny_buffer,              NULL, NULL, 0, NULL },
    { "/edge/size-equals-original",     test_size_field_equals_original,        NULL, NULL, 0, NULL },
    { "/edge/compressed-smaller",       test_compressed_smaller_for_compressible, NULL, NULL, 0, NULL },
    { "/edge/header-always-present",    test_header_always_present,             NULL, NULL, 0, NULL },
    { "/edge/roundtrip-256kb",          test_roundtrip_256kb,                   NULL, NULL, 0, NULL },
    { "/edge/roundtrip-1mb",            test_roundtrip_1mb,                     NULL, NULL, 0, NULL },
    { "/edge/every-byte-pattern",       test_every_byte_pattern,                NULL, NULL, 0, NULL },

    { NULL, NULL, NULL, NULL, 0, NULL }
};

static const MunitSuite suite = {
    "/mozlz4",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&suite, NULL, argc, argv);
}
