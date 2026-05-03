/*
 * fuzz_mozlz4.c : libFuzzer harness for mozlz4
 *
 * build: clang -fsanitize=fuzzer,address -Ilz4 -Isrc -o fuzz_mozlz4 \
 *            test/fuzz_mozlz4.c src/mozlz4.c lz4/lz4.c
 * run:   ./fuzz_mozlz4
 *
 * exercises the decompression path (the primary attack surface)
 * with coverage-guided random inputs.
 */

#include "mozlz4.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint32_t decompressed_size = 0;
    int rc = mozlz4_read_size(data, size, &decompressed_size);

    if (rc == MOZLZ4_OK && decompressed_size > 0) {
        /* cap at 16MB so the fuzzer doesn't OOM on crafted headers */
        size_t cap = decompressed_size > (16 << 20) ? (16 << 20) : (size_t)decompressed_size;
        uint8_t *out = (uint8_t *)malloc(cap);
        if (out) {
            size_t out_len = 0;
            mozlz4_decompress(data, size, out, &out_len, cap);
            free(out);
        }
    }

    /* also try compressing small inputs */
    if (size <= (1 << 16)) {
        size_t bound = mozlz4_compress_bound(size);
        if (bound > 0) {
            uint8_t *compressed = (uint8_t *)malloc(bound);
            if (compressed) {
                size_t comp_len = 0;
                mozlz4_compress(data, size, compressed, &comp_len, bound);

                if (comp_len > 0) {
                    uint8_t *decompressed = (uint8_t *)malloc(size + 64);
                    if (decompressed) {
                        size_t decomp_len = 0;
                        mozlz4_decompress(compressed, comp_len,
                                         decompressed, &decomp_len, size + 64);
                        free(decompressed);
                    }
                }
                free(compressed);
            }
        }
    }

    return 0;
}
