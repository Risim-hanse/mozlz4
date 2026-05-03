/*
 * mozlz4.c : Mozilla LZ4 container format implementation
 *
 * handles the "mozLz40\0" header + LZ4 block that Firefox uses
 * for search.json.mozlz4 and similar files. behavior matches
 * the Rust mozlz4 crate (jusw85/mozlz4) exactly.
 */

#include "mozlz4.h"
#include "lz4.h"

#include <string.h>
#include <limits.h>

static const uint8_t MOZLZ4_MAGIC_BYTES[MOZLZ4_MAGIC_LEN] = {
    'm', 'o', 'z', 'L', 'z', '4', '0', '\0'
};

/* anything larger than this overflows int when passed to LZ4 */
#define MOZLZ4_MAX_INPUT LZ4_MAX_INPUT_SIZE

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* shared magic validation for decompress and read_size */
static int check_magic(const uint8_t *in, size_t in_len)
{
    if (in_len < MOZLZ4_HEADER_LEN)
        return MOZLZ4_ERR_TOO_SHORT;
    if (memcmp(in, MOZLZ4_MAGIC_BYTES, MOZLZ4_MAGIC_LEN) != 0)
        return MOZLZ4_ERR_MAGIC;
    return MOZLZ4_OK;
}

MOZLZ4_API int mozlz4_decompress(const uint8_t *in, size_t in_len,
                                 uint8_t *out, size_t *out_len,
                                 size_t out_capacity)
{
    uint32_t decompressed_size;
    int bytes_decompressed;
    const uint8_t *block;
    size_t block_len;
    int magic_rc;

    if (!in || !out || !out_len)
        return MOZLZ4_ERR_NULL;

    magic_rc = check_magic(in, in_len);
    if (magic_rc != MOZLZ4_OK)
        return magic_rc;

    decompressed_size = read_u32_le(in + MOZLZ4_MAGIC_LEN);

    if (out_capacity < decompressed_size)
        return MOZLZ4_ERR_DECOMPRESS;

    block = in + MOZLZ4_HEADER_LEN;
    block_len = in_len - MOZLZ4_HEADER_LEN;

    /* empty file, nothing to decompress */
    if (decompressed_size == 0) {
        *out_len = 0;
        return MOZLZ4_OK;
    }

    /*
     * LZ4_decompress_safe takes int parameters. block_len could be
     * larger than INT_MAX on 64-bit, and decompressed_size could be
     * anywhere up to UINT32_MAX. Either one wrapping negative would
     * make LZ4 go sideways, so we catch that here.
     */
    if (block_len > (size_t)INT_MAX)
        return MOZLZ4_ERR_TOO_LARGE;
    if (decompressed_size > (uint32_t)INT_MAX)
        return MOZLZ4_ERR_TOO_LARGE;

    bytes_decompressed = LZ4_decompress_safe(
        (const char *)block,
        (char *)out,
        (int)block_len,
        (int)decompressed_size
    );

    if (bytes_decompressed < 0)
        return MOZLZ4_ERR_DECOMPRESS;

    *out_len = (size_t)bytes_decompressed;
    return MOZLZ4_OK;
}

MOZLZ4_API int mozlz4_compress(const uint8_t *in, size_t in_len,
                               uint8_t *out, size_t *out_len,
                               size_t out_capacity)
{
    int lz4_bound;
    int bytes_compressed;
    size_t needed;

    if (!in || !out || !out_len)
        return MOZLZ4_ERR_NULL;

    if (in_len > MOZLZ4_MAX_INPUT)
        return MOZLZ4_ERR_TOO_LARGE;

    /* safe cast: we just checked in_len fits in int */
    lz4_bound = LZ4_compressBound((int)in_len);
    if (lz4_bound <= 0)
        return MOZLZ4_ERR_COMPRESS;

    needed = MOZLZ4_HEADER_LEN + (size_t)lz4_bound;
    if (out_capacity < needed)
        return MOZLZ4_ERR_COMPRESS;

    memcpy(out, MOZLZ4_MAGIC_BYTES, MOZLZ4_MAGIC_LEN);
    write_u32_le(out + MOZLZ4_MAGIC_LEN, (uint32_t)in_len);

    bytes_compressed = LZ4_compress_default(
        (const char *)in,
        (char *)(out + MOZLZ4_HEADER_LEN),
        (int)in_len,
        lz4_bound
    );

    if (bytes_compressed <= 0)
        return MOZLZ4_ERR_COMPRESS;

    *out_len = MOZLZ4_HEADER_LEN + (size_t)bytes_compressed;
    return MOZLZ4_OK;
}

MOZLZ4_API size_t mozlz4_compress_bound(size_t input_size)
{
    int lz4_bound;

    if (input_size > MOZLZ4_MAX_INPUT)
        return 0;

    lz4_bound = LZ4_compressBound((int)input_size);
    if (lz4_bound <= 0)
        return 0;
    return MOZLZ4_HEADER_LEN + (size_t)lz4_bound;
}

MOZLZ4_API int mozlz4_read_size(const uint8_t *in, size_t in_len,
                                uint32_t *out_size)
{
    int magic_rc;

    if (!out_size)
        return MOZLZ4_ERR_NULL;

    magic_rc = check_magic(in, in_len);
    if (magic_rc != MOZLZ4_OK) {
        *out_size = 0;
        return magic_rc;
    }

    *out_size = read_u32_le(in + MOZLZ4_MAGIC_LEN);
    return MOZLZ4_OK;
}
