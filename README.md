# mozlz4

a little C tool for Firefox's `search.json.mozlz4` files.
no dependencies, no build systems, just `make` and go.

## what it does

Firefox stores your search engines in `search.json.mozlz4`. it's JSON, but wrapped in a small LZ4 container with a custom header. if you ever want to peek inside, edit your search engines by hand, or script something with them, you need something to pack and unpack it.

the format is just:

```
"mozLz40\0" (8 bytes) + decompressed size (4 bytes, LE) + LZ4 block
```

twelve bytes of header. that's the whole trick.

## quick start

```bash
make && make test
```

then:

```bash
# decompress
./mozlz4 search.json.mozlz4 search.json

# compress back
./mozlz4 -z search.json search.json.mozlz4

# pipe around
cat search.json.mozlz4 | ./mozlz4 -x - | jq .
```

`-x` extracts (default), `-z` compresses, `-h` shows help. use `-` for stdin or stdout.

## pre-built binaries

push a `v*` tag and CI builds for Linux, Windows, and macOS (x86_64 + arm64). they show up as release artifacts, all built with `-O2`.

## API

`src/mozlz4.h` if you want to use it as a library:

```c
int mozlz4_decompress(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len, size_t out_capacity);

int mozlz4_compress(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len, size_t out_capacity);

size_t mozlz4_compress_bound(size_t input_size);
uint32_t mozlz4_read_size(const uint8_t *in, size_t in_len, int *ok);
```

`MOZLZ4_OK` on success, negative error code on failure. details in the header~

## layout

```
src/mozlz4.h          the API
src/mozlz4.c          format implementation
src/mozlz4_cli.c      CLI
lz4/                   LZ4 v1.9.3, vendored from Mozilla's gecko-dev
test/test_mozlz4.c    37 tests
```

## why C

the Rust crate that does this (`jusw85/mozlz4`) is mostly a wrapper around LZ4, which is C anyway. this version skips the wrapper layer and talks to LZ4 directly. same behavior, one `gcc` call, no cargo needed.

## license

mozlz4 wrapper: public domain. LZ4: BSD-2-Clause by Yann Collet.
