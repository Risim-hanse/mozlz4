# mozlz4

a small C tool for compressing and decompressing Firefox's `search.json.mozlz4` files.

## wait, what's a mozlz4?

so Firefox stores your search engines (Google, DuckDuckGo, that one custom one you made) in a file called `search.json.mozlz4` inside your profile folder. it's just JSON, but Mozilla wrapped it in a custom LZ4 container with an 8-byte magic header. why? honestly... not sure. the file is tiny anyway. but that's how it works, and if you want to edit those search engines by hand, you need a tool to unpack and repack it.

the format is simple:

```
[8 bytes: "mozLz40\0"][4 bytes: decompressed size, little-endian][LZ4 compressed data]
```

that's it. 12 bytes of header, then a stock LZ4 block.

## usage

```bash
# decompress (default)
./mozlz4 search.json.mozlz4 search.json

# compress back
./mozlz4 -z search.json search.json.mozlz4

# read from stdin, write to stdout
cat search.json.mozlz4 | ./mozlz4 -x - | jq .
```

flags:
- `-x, --extract` : decompress (this is the default)
- `-z, --compress` : compress
- `-h, --help` : you know what this does

use `-` for stdin on input or stdout on output.

## building

```bash
make          # builds mozlz4 + test_mozlz4
make test     # builds and runs the test suite
make clean    # cleans up
```

needs `gcc` (or `clang` for fuzzing). works on Linux, macOS, and Windows (via MSYS2/MinGW).

### fuzzing

```bash
make fuzz     # builds and runs libFuzzer harness for 60 seconds
```

needs `clang`. exercises the decompression path with coverage-guided random inputs.

## pre-built binaries

grab the latest release from the [releases page](https://github.com/Risim-hanse/mozlz4/releases).

or push a tag and the CI builds everything:

```bash
git tag v0.1.0-alpha.1
git push origin v0.1.0-alpha.1
```

you'll get:
- **Linux x86_64** : `.tar.gz`
- **Windows x86_64** : `.zip`
- **macOS x86_64** : `.tar.gz`
- **macOS arm64** : `.tar.gz`

all built with `-O2`, no `-march=native`, so they'll run basically anywhere.

## versioning

this project uses [semantic versioning](https://semver.org/):

- `0.x.y` -- pre-stable, API may change between minor versions
- `0.x.y-alpha.N` -- alpha pre-release, expect breakage
- `0.x.y-rc.N` -- release candidate, API frozen, testing only
- `1.0.0` and later -- stable, API backward-compatible within a major version

the version in `src/mozlz4.h` reflects the current development state.
tags are the source of truth for releases.

## API

if you want to use this as a library, the header is `src/mozlz4.h`:

```c
#include "mozlz4.h"

// decompress a mozlz4 buffer
int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity);

// compress a buffer into mozlz4 format
int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity);

// how big does the output buffer need to be?
size_t mozlz4_compress_bound(size_t input_size);

// just read the decompressed size without touching the data
int mozlz4_read_size(const uint8_t *in, size_t in_len, uint32_t *out_size);
```

returns `MOZLZ4_OK` on success, negative error code on failure. the error codes are in the header.

## project structure

```
├── src/
│   ├── mozlz4.h          public API
│   ├── mozlz4.c          format implementation
│   └── mozlz4_cli.c      CLI tool
├── lz4/
│   ├── lz4.h             LZ4 v1.9.3 (vendored from Mozilla's gecko-dev)
│   └── lz4.c
├── test/
│   ├── test_mozlz4.c     test suite (48 tests)
│   ├── fuzz_mozlz4.c     libFuzzer harness
│   └── vendor/
│       ├── munit.c        mu unit testing framework v0.4.1
│       └── munit.h
├── Makefile
└── .github/workflows/
    ├── build.yml          CI for Linux, Windows, macOS
    └── codeql.yml         static analysis
```

## why C and not Rust?

there's a Rust crate (`jusw85/mozlz4`) that does the same thing. the catch is that the actual compression work is done by LZ4, which is a C library. the Rust version was mostly a wrapper around the same C code, just with a Rust build system on top. this version cuts out the middle layer. same behavior, fewer moving parts, compiles with a single `gcc` call.

## dependencies

none. LZ4 v1.9.3 is vendored in `lz4/`, pulled from Mozilla's gecko-dev tree.

## contributors

- **Risim-hanse** ~> original C implementation, build system, CI, fuzzing
- **Lumine** ~> code cleanup, comment style, README polish

## license

LZ4 is BSD-2-Clause by Yann Collet. munit is MIT by Evan Nemerson. the mozlz4 wrapper code is public domain.
