/*
 * mozlz4.c — Mozilla LZ4 container format implementation
 *
 * Behavior matches the Rust mozlz4 crate (jusw85/mozlz4) exactly.
 */

#include "mozlz4.h"
#include "lz4.h"

#include <string.h>

/* The magic number as raw bytes: 'm','o','z','L','z','4','0','\0' */
static const uint8_t MOZLZ4_MAGIC_BYTES[MOZLZ4_MAGIC_LEN] = {
    'm', 'o', 'z', 'L', 'z', '4', '0', '\0'
};

/* Read a uint32 little-endian from a byte buffer */
static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Write a uint32 little-endian to a byte buffer */
static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity)
{
    uint32_t decompressed_size;
    int bytes_decompressed;
    const uint8_t *block;
    size_t block_len;

    if (!in || !out || !out_len)
        return MOZLZ4_ERR_MAGIC;

    /* Must be at least header length */
    if (in_len < MOZLZ4_HEADER_LEN)
        return MOZLZ4_ERR_TOO_SHORT;

    /* Validate magic number */
    if (memcmp(in, MOZLZ4_MAGIC_BYTES, MOZLZ4_MAGIC_LEN) != 0)
        return MOZLZ4_ERR_MAGIC;

    /* Read decompressed size from header */
    decompressed_size = read_u32_le(in + MOZLZ4_MAGIC_LEN);

    /* Check output buffer is large enough */
    if (out_capacity < decompressed_size)
        return MOZLZ4_ERR_DECOMPRESS;

    /* LZ4 compressed block starts after header */
    block = in + MOZLZ4_HEADER_LEN;
    block_len = in_len - MOZLZ4_HEADER_LEN;

    /* Handle edge case: header-only file with 0 decompressed size */
    if (decompressed_size == 0) {
        *out_len = 0;
        return MOZLZ4_OK;
    }

    /* Decompress */
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

int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity)
{
    int compress_bound;
    int bytes_compressed;
    size_t needed;

    if (!in || !out || !out_len)
        return MOZLZ4_ERR_COMPRESS;

    compress_bound = LZ4_compressBound((int)in_len);
    if (compress_bound <= 0)
        return MOZLZ4_ERR_COMPRESS;

    needed = MOZLZ4_HEADER_LEN + (size_t)compress_bound;
    if (out_capacity < needed)
        return MOZLZ4_ERR_COMPRESS;

    /* Write header */
    memcpy(out, MOZLZ4_MAGIC_BYTES, MOZLZ4_MAGIC_LEN);
    write_u32_le(out + MOZLZ4_MAGIC_LEN, (uint32_t)in_len);

    /* Compress data into output buffer after header */
    bytes_compressed = LZ4_compress_default(
        (const char *)in,
        (char *)(out + MOZLZ4_HEADER_LEN),
        (int)in_len,
        compress_bound
    );

    if (bytes_compressed <= 0)
        return MOZLZ4_ERR_COMPRESS;

    *out_len = MOZLZ4_HEADER_LEN + (size_t)bytes_compressed;
    return MOZLZ4_OK;
}

size_t mozlz4_compress_bound(size_t input_size)
{
    int lz4_bound = LZ4_compressBound((int)input_size);
    if (lz4_bound <= 0)
        return 0;
    return MOZLZ4_HEADER_LEN + (size_t)lz4_bound;
}

uint32_t mozlz4_read_size(const uint8_t *in, size_t in_len, int *ok)
{
    if (!in || in_len < MOZLZ4_HEADER_LEN ||
        memcmp(in, MOZLZ4_MAGIC_BYTES, MOZLZ4_MAGIC_LEN) != 0) {
        if (ok) *ok = 0;
        return 0;
    }
    if (ok) *ok = 1;
    return read_u32_le(in + MOZLZ4_MAGIC_LEN);
}
