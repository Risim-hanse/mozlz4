# mozlz4: Mozilla LZ4 container format in C

Compress and decompress Firefox `search.json.mozlz4` files.

## Building locally

```
make          # builds mozlz4 + test_mozlz4
make test     # builds and runs tests
make clean    # removes build artifacts
```

Works on Linux, macOS, and MSYS2/MinGW on Windows. Requires `gcc`.

## CI / Pre-built binaries

Push a tag to trigger builds for all platforms:

```bash
git tag v1.0.0
git push origin v1.0.0
```

This produces binaries for:
- **Linux x86_64** — `.tar.gz`
- **Windows x86_64** — `.zip`
- **macOS x86_64** — `.tar.gz`
- **macOS arm64** — `.tar.gz`

Binaries appear as release artifacts. The workflow uses MSYS2 UCRT64 on Windows,
plain gcc on Linux, and Apple clang on macOS. All use `-O2`, no `-march=native`.

## Usage

```
./mozlz4 -x input.mozlz4 [output]     decompress (default)
./mozlz4 -z input [output.mozlz4]     compress
./mozlz4 -x - [output]                decompress from stdin
./mozlz4 -z input -                   compress to stdout
```

## File Format

```
Offset  Size  Description
0       8     Magic: "mozLz40\0"
8       4     Decompressed size (uint32, little-endian)
12      N     LZ4 compressed block
```

## API

```c
#include "mozlz4.h"

int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity);

int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity);

size_t mozlz4_compress_bound(size_t input_size);
uint32_t mozlz4_read_size(const uint8_t *in, size_t in_len, int *ok);
```

Returns `MOZLZ4_OK` on success, negative error code on failure.

## Project Structure

```
├── Makefile
├── README.md
├── src/
│   ├── mozlz4.h          # Public API
│   ├── mozlz4.c          # Format implementation
│   └── mozlz4_cli.c      # CLI tool
├── lz4/
│   ├── lz4.h             # LZ4 v1.9.3 (from Mozilla gecko-dev)
│   └── lz4.c             # LZ4 v1.9.3 (from Mozilla gecko-dev)
└── test/
    └── test_mozlz4.c     # Test suite
```

## Dependencies

None. LZ4 is included (stock v1.9.3 from Mozilla's gecko-dev).

## License

LZ4 is BSD-2-Clause by Yann Collet. The mozlz4 wrapper code is public domain.
