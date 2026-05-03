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

#define MOZLZ4_VERSION_MAJOR  0
#define MOZLZ4_VERSION_MINOR  1
#define MOZLZ4_VERSION_PATCH  0
#define MOZLZ4_VERSION_PRE    "alpha"
#define MOZLZ4_VERSION_STRING "0.1.0-alpha"

/* visibility macro for shared library builds */
#if defined(MOZLZ4_SHARED) && defined(_WIN32)
#  if defined(MOZLZ4_BUILDING)
#    define MOZLZ4_API __declspec(dllexport)
#  else
#    define MOZLZ4_API __declspec(dllimport)
#  endif
#elif defined(MOZLZ4_SHARED) && defined(__GNUC__) && __GNUC__ >= 4
#  define MOZLZ4_API __attribute__((visibility("default")))
#else
#  define MOZLZ4_API
#endif

#define MOZLZ4_OK              0
#define MOZLZ4_ERR_NULL       -1
#define MOZLZ4_ERR_MAGIC      -2
#define MOZLZ4_ERR_TOO_SHORT  -3
#define MOZLZ4_ERR_DECOMPRESS -4
#define MOZLZ4_ERR_COMPRESS   -5
#define MOZLZ4_ERR_TOO_LARGE  -6

#define MOZLZ4_MAGIC      "mozLz40"
#define MOZLZ4_MAGIC_LEN  8
#define MOZLZ4_HEADER_LEN 12

/*
 * decompress a mozlz4 buffer.
 * in must be at least MOZLZ4_HEADER_LEN bytes.
 * out buffer must be >= the decompressed_size stored in the header.
 */
MOZLZ4_API int mozlz4_decompress(const uint8_t *in, size_t in_len,
                                 uint8_t *out, size_t *out_len,
                                 size_t out_capacity);

/*
 * compress a buffer into mozlz4 format.
 * out buffer should be at least MOZLZ4_HEADER_LEN + LZ4_compressBound(in_len).
 * returns MOZLZ4_ERR_TOO_LARGE if in_len exceeds LZ4_MAX_INPUT_SIZE.
 */
MOZLZ4_API int mozlz4_compress(const uint8_t *in, size_t in_len,
                               uint8_t *out, size_t *out_len,
                               size_t out_capacity);

/* max compressed size for a given input size. returns 0 if too large. */
MOZLZ4_API size_t mozlz4_compress_bound(size_t input_size);

/*
 * read decompressed size from header without decompressing.
 * returns MOZLZ4_OK on success, negative error code on failure.
 */
MOZLZ4_API int mozlz4_read_size(const uint8_t *in, size_t in_len,
                                uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* MOZLZ4_H */
