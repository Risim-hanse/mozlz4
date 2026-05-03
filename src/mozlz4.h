/*
 * mozlz4.h — Mozilla LZ4 container format (mozLz40)
 *
 * Format: [8-byte magic "mozLz40\0"][4-byte LE decompressed_size][LZ4 block data]
 *
 * This is a clean C implementation matching the Rust mozlz4 crate behavior.
 */

#ifndef MOZLZ4_H
#define MOZLZ4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
#define MOZLZ4_OK              0
#define MOZLZ4_ERR_MAGIC      -1   /* Bad or missing magic number */
#define MOZLZ4_ERR_TOO_SHORT  -2   /* Input shorter than header (12 bytes) */
#define MOZLZ4_ERR_DECOMPRESS -3   /* LZ4 decompression failed */
#define MOZLZ4_ERR_COMPRESS   -4   /* LZ4 compression failed */

/* Magic number: "mozLz40\0" */
#define MOZLZ4_MAGIC      "mozLz40"
#define MOZLZ4_MAGIC_LEN  8   /* including the null terminator */

/* Header size: magic(8) + size_field(4) */
#define MOZLZ4_HEADER_LEN  12

/*
 * Decompress a mozlz4 buffer.
 *
 * Parameters:
 *   in           - Input buffer containing mozlz4 data
 *   in_len       - Length of input buffer in bytes
 *   out          - Output buffer (caller-allocated)
 *   out_len      - On success: actual decompressed size written
 *   out_capacity - Size of output buffer
 *
 * Returns:
 *   MOZLZ4_OK on success, negative error code on failure.
 *
 * Notes:
 *   - Input must be at least MOZLZ4_HEADER_LEN bytes
 *   - Magic number must be "mozLz40\0"
 *   - Output buffer must be at least as large as the decompressed_size field
 */
int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity);

/*
 * Compress a buffer into mozlz4 format.
 *
 * Parameters:
 *   in           - Input (uncompressed) buffer
 *   in_len       - Length of input buffer in bytes
 *   out          - Output buffer (caller-allocated)
 *   out_len      - On success: actual compressed size written (header + data)
 *   out_capacity - Size of output buffer
 *
 * Returns:
 *   MOZLZ4_OK on success, negative error code on failure.
 *
 * Notes:
 *   - Output buffer should be at least MOZLZ4_HEADER_LEN + LZ4_compressBound(in_len)
 *   - Uses LZ4_compress_default (same as the Rust version)
 */
int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity);

/*
 * Get the maximum compressed size for a given input size.
 * Returns the total mozlz4 file size (header + worst-case LZ4 data).
 * Returns 0 if input_size is too large.
 */
size_t mozlz4_compress_bound(size_t input_size);

/*
 * Read the decompressed size from a mozlz4 header without decompressing.
 * Returns 0 on error (bad magic / too short), otherwise the size value.
 * Note: size 0 is valid but also used as error — check *ok flag.
 */
uint32_t mozlz4_read_size(const uint8_t *in, size_t in_len, int *ok);

#ifdef __cplusplus
}
#endif

#endif /* MOZLZ4_H */
