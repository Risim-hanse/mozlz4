CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Ilz4 -Isrc
LDFLAGS =

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -s -Wl,--gc-sections -Wl,-z,norelro
  CFLAGS  += -fno-math-errno -fno-stack-protector -fomit-frame-pointer \
             -ffunction-sections -fdata-sections
  EXE     =
else ifeq ($(UNAME_S),Darwin)
  LDFLAGS += -dead_strip
  CFLAGS  += -fno-math-errno -fomit-frame-pointer \
             -ffunction-sections -fdata-sections
  EXE     =
else
  # Windows / MSYS2 / MinGW
  LDFLAGS += -s -Wl,--gc-sections
  CFLAGS  += -fno-math-errno -fno-stack-protector -fomit-frame-pointer \
             -ffunction-sections -fdata-sections
  EXE     = .exe
endif

CLI     = mozlz4$(EXE)
TEST    = test_mozlz4$(EXE)

.PHONY: all clean test

all: $(CLI) $(TEST)

$(CLI): src/mozlz4_cli.c src/mozlz4.c src/mozlz4.h lz4/lz4.c lz4/lz4.h
	$(CC) $(CFLAGS) -o $@ src/mozlz4_cli.c src/mozlz4.c lz4/lz4.c $(LDFLAGS)

$(TEST): test/test_mozlz4.c src/mozlz4.c src/mozlz4.h lz4/lz4.c lz4/lz4.h
	$(CC) $(CFLAGS) -o $@ test/test_mozlz4.c src/mozlz4.c lz4/lz4.c $(LDFLAGS)

test: $(TEST)
	./$(TEST)

clean:
	rm -f mozlz4 mozlz4.exe test_mozlz4 test_mozlz4.exe
