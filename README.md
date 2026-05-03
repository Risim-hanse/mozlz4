# mozlz4

small C tool for packing and unpacking Firefox `search.json.mozlz4` files. no dependencies, just gcc and make~

## okay so

Firefox keeps your search engines in this file called `search.json.mozlz4`. it's just JSON but they wrapped it in a little LZ4 container with a custom header. the format is honestly kinda cute, it's just twelve bytes of header ("mozLz40\0" plus a uint32 for decompressed size) and then a stock LZ4 block. that's it.

if you ever want to edit your search engines by hand or peek at what Firefox has stored, you need something to unpack this. most existing tools are either Python scripts or that Rust crate which... is mostly just wrapping the C LZ4 library anyway. so here's a C version that talks to LZ4 directly.

## usage

```bash
./mozlz4 search.json.mozlz4 search.json        # decompress
./mozlz4 -z search.json search.json.mozlz4      # compress back
cat search.json.mozlz4 | ./mozlz4 -x - | jq .   # pipe it
```

`-x` extracts (default), `-z` compresses. use `-` for stdin/stdout.

## building

```bash
make && make test
```

needs gcc. works on Linux, macOS, Windows (MSYS2).

## releases

push a `v*` tag and CI builds everything for Linux, Windows, macOS (x86_64 + arm64). binaries show up as release artifacts~

## API

`src/mozlz4.h` has the full API if you want to use it as a library. `MOZLZ4_OK` on success, negative error codes on failure. check the header, it's pretty short.

## layout

```
src/mozlz4.h        the API
src/mozlz4.c        format implementation
src/mozlz4_cli.c    CLI tool
lz4/                 LZ4 v1.9.3 vendored from Mozilla's gecko-dev
test/test_mozlz4.c  37 tests
```

## why C

the Rust crate (`jusw85/mozlz4`) is fine but it felt like wearing a coat indoors. LZ4 is C, the format is simple, the whole thing is like 500 lines. cargo and build.rs felt like overkill. this version is just... the thing itself. one gcc call and you're done~

## license

mozlz4 wrapper is public domain. LZ4 is BSD-2-Clause by Yann Collet.
