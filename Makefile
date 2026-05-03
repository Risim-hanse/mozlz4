CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wconversion -Wsign-conversion \
          -std=c99 -pedantic -Ilz4 -Isrc
LDFLAGS =

# make SANITIZE=1 for an ASan+UBSan build
ifdef SANITIZE
  CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined
endif

# make ARCH=arm64 (or x86_64) for cross-compilation on macOS
ifdef ARCH
  CFLAGS += -arch $(ARCH)
endif

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -s -Wl,--gc-sections -Wl,-z,relro,-z,now
  CFLAGS  += -fno-math-errno -fstack-protector-strong -fomit-frame-pointer \
             -ffunction-sections -fdata-sections
  EXE     =
else ifeq ($(UNAME_S),Darwin)
  LDFLAGS += -dead_strip
  CFLAGS  += -fno-math-errno -fstack-protector-strong -fomit-frame-pointer \
             -ffunction-sections -fdata-sections
  EXE     =
else
  LDFLAGS += -s -Wl,--gc-sections
  CFLAGS  += -fno-math-errno -fstack-protector-strong -fomit-frame-pointer \
             -ffunction-sections -fdata-sections
  EXE     = .exe
endif

CLI     = mozlz4$(EXE)
TEST    = test_mozlz4$(EXE)
FUZZ    = fuzz_mozlz4$(EXE)

# vendored code: keep platform/arch flags from CFLAGS but drop strict warnings.
# use -std=c11 so munit's C11 atomics work (ATOMIC_VAR_INIT is gone in C23).
VENDOR_CFLAGS = $(filter-out -Wconversion -Wsign-conversion -std=% -pedantic, $(CFLAGS)) -std=c11

lz4/lz4.o: lz4/lz4.c lz4/lz4.h
	$(CC) $(VENDOR_CFLAGS) -Ilz4 -c -o $@ lz4/lz4.c

test/vendor/munit.o: test/vendor/munit.c test/vendor/munit.h
	$(CC) $(VENDOR_CFLAGS) -c -o $@ test/vendor/munit.c

.PHONY: all clean test fuzz

all: $(CLI) $(TEST)

$(CLI): src/mozlz4_cli.c src/mozlz4.c src/mozlz4.h lz4/lz4.o
	$(CC) $(CFLAGS) -o $@ src/mozlz4_cli.c src/mozlz4.c lz4/lz4.o $(LDFLAGS)

$(TEST): test/test_mozlz4.c test/vendor/munit.o \
         src/mozlz4.c src/mozlz4.h lz4/lz4.o
	$(CC) $(CFLAGS) -Itest -o $@ test/test_mozlz4.c test/vendor/munit.o src/mozlz4.c lz4/lz4.o $(LDFLAGS)

test: $(TEST)
	./$(TEST)

fuzz:
	clang -O2 -g -fsanitize=fuzzer,address -Ilz4 -Isrc \
		-o $(FUZZ) test/fuzz_mozlz4.c src/mozlz4.c lz4/lz4.c
	./$(FUZZ) -max_total_time=60

clean:
	rm -f mozlz4 mozlz4.exe test_mozlz4 test_mozlz4.exe fuzz_mozlz4 fuzz_mozlz4.exe lz4/lz4.o test/vendor/munit.o
