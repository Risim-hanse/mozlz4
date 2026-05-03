/*
 * mozlz4.h : Mozilla LZ4 container format (mozLz40)
 *
 * the format is just LZ4 with a custom header:
 *   [8 bytes magic "mozLz40\0"][4 bytes LE decompressed size][LZ4 block]
 *
 * this is a clean C implementation matching the Rust mozlz4 crate behavior.
 */

#ifndef MOZLZ4_H
#define MOZLZ4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* return codes */
#define MOZLZ4_OK              0
#define MOZLZ4_ERR_MAGIC      -1   /* bad or missing magic number */
#define MOZLZ4_ERR_TOO_SHORT  -2   /* input shorter than header (12 bytes) */
#define MOZLZ4_ERR_DECOMPRESS -3   /* LZ4 decompression failed */
#define MOZLZ4_ERR_COMPRESS   -4   /* LZ4 compression failed */

/* magic: "mozLz40\0" : 8 bytes including the null terminator */
#define MOZLZ4_MAGIC      "mozLz40"
#define MOZLZ4_MAGIC_LEN  8

/* header is magic (8) + size field (4) = 12 bytes total */
#define MOZLZ4_HEADER_LEN  12

/*
 * decompress a mozlz4 buffer.
 *
 * in must be at least MOZLZ4_HEADER_LEN bytes.
 * out buffer must be >= the decompressed_size stored in the header.
 * returns MOZLZ4_OK on success.
 */
int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity);

/*
 * compress a buffer into mozlz4 format.
 *
 * out buffer should be at least MOZLZ4_HEADER_LEN + LZ4_compressBound(in_len).
 * returns MOZLZ4_OK on success.
 */
int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity);

/*
 * max compressed size for a given input size (header + worst-case LZ4).
 * returns 0 if input_size is too large.
 */
size_t mozlz4_compress_bound(size_t input_size);

/*
 * read the decompressed size from a mozlz4 header without decompressing.
 * returns 0 on error (bad magic / too short).
 * note: size 0 is valid but also used as error, so check the *ok flag.
 */
uint32_t mozlz4_read_size(const uint8_t *in, size_t in_len, int *ok);

#ifdef __cplusplus
}
#endif

#endif /* MOZLZ4_H */
