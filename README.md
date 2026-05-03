# mozlz4

compress and decompress Firefox `search.json.mozlz4` files. pure C, zero dependencies, ~500 lines.

## so what is this

Firefox keeps your search engines in a file called `search.json.mozlz4`. it's just JSON, but wrapped in a custom LZ4 container with a weird 8-byte header. if you ever want to edit those search engines by hand, or script something with them, or just peek inside... you need a tool to unpack it.

the whole format is:

```
[8 bytes: "mozLz40\0"][4 bytes: decompressed size, LE][LZ4 block]
```

twelve bytes of header, then stock LZ4. that's it. honestly a little strange that Firefox bothered compressing a file this small, but here we are.

## usage

```bash
# decompress (this is the default)
./mozlz4 search.json.mozlz4 search.json

# compress back
./mozlz4 -z search.json search.json.mozlz4

# pipe it
cat search.json.mozlz4 | ./mozlz4 -x - | jq .
```

flags: `-x` to extract, `-z` to compress, `-h` for help. use `-` for stdin or stdout.

## building

```bash
make          # builds mozlz4 and the test suite
make test     # builds and runs tests
make clean    # cleans up
```

needs `gcc`. works on Linux, macOS, and Windows via MSYS2/MinGW.

## pre-built binaries

push a tag and CI builds everything:

```bash
git tag v1.0.0
git push origin v1.0.0
```

artifacts: Linux x86_64 (`.tar.gz`), Windows x86_64 (`.zip`), macOS x86_64 and arm64 (`.tar.gz`). all `-O2`, no `-march=native`, so they run on pretty much anything.

## API

`src/mozlz4.h` is a single-header-ish C API if you want to use this as a library:

```c
#include "mozlz4.h"

int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity);

int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity);

size_t mozlz4_compress_bound(size_t input_size);

uint32_t mozlz4_read_size(const uint8_t *in, size_t in_len, int *ok);
```

`MOZLZ4_OK` on success, negative error code on failure. check the header for details.

## project layout

```
src/
  mozlz4.h          public API
  mozlz4.c          format implementation
  mozlz4_cli.c      CLI tool
lz4/
  lz4.h, lz4.c      LZ4 v1.9.3, vendored from Mozilla's gecko-dev
test/
  test_mozlz4.c      37 tests across 6 groups
Makefile
.github/workflows/
  build.yml           CI: Linux, Windows, macOS
```

## why C instead of Rust

there's a Rust crate that does the same thing (`jusw85/mozlz4`). the thing is, the actual compression is all LZ4, which is C. the Rust version was mostly a build system and a thin wrapper. this version talks directly to LZ4. same behavior, one `gcc` call, no cargo, no build.rs, no dependency tree.

## license

the mozlz4 wrapper is public domain. LZ4 is BSD-2-Clause by Yann Collet (see `lz4/` for details).
