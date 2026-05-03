/*
 * mozlz4_cli.c — CLI for mozlz4 compress/decompress
 *
 * drop-in replacement for the Rust mozlz4-bin tool.
 *
 * usage:
 *   mozlz4 [-x] input [output]    decompress (default)
 *   mozlz4 -z input [output]      compress
 *
 * use '-' for stdin (input) or stdout (output).
 */

#include "mozlz4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog)
{
    fprintf(stderr,
        "mozlz4 - compress and decompress mozlz4 files (Firefox search.json)\n"
        "\n"
        "USAGE:\n"
        "  %s [-x] <input> [output]    decompress mozlz4 (default)\n"
        "  %s -z <input> [output]      compress to mozlz4\n"
        "\n"
        "FLAGS:\n"
        "  -x, --extract     decompress mozlz4 (default)\n"
        "  -z, --compress    compress to mozlz4\n"
        "  -h, --help        print this help\n"
        "\n"
        "Use '-' for stdin (input) or stdout (output).\n"
        "Default output is stdout.\n",
        prog, prog);
}

/* read an entire file into a malloc'd buffer */
static uint8_t *read_all(FILE *f, size_t *out_len)
{
    size_t cap = 8192;
    size_t len = 0;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) return NULL;

    for (;;) {
        if (len == cap) {
            cap *= 2;
            uint8_t *newbuf = (uint8_t *)realloc(buf, cap);
            if (!newbuf) { free(buf); return NULL; }
            buf = newbuf;
        }
        size_t n = fread(buf + len, 1, cap - len, f);
        len += n;
        if (n == 0) break;
    }
    *out_len = len;
    return buf;
}

/* write all bytes to a file, retrying on partial writes */
static int write_all(FILE *f, const uint8_t *data, size_t len)
{
    while (len > 0) {
        size_t n = fwrite(data, 1, len, f);
        if (n == 0) return -1;
        data += n;
        len -= n;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int do_compress = 0;
    const char *input_path = NULL;
    const char *output_path = "-";
    int argi = 1;

    /* parse flags */
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-x") == 0 || strcmp(argv[argi], "--extract") == 0) {
            do_compress = 0;
            argi++;
        } else if (strcmp(argv[argi], "-z") == 0 || strcmp(argv[argi], "--compress") == 0) {
            do_compress = 1;
            argi++;
        } else if (strcmp(argv[argi], "-h") == 0 || strcmp(argv[argi], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[argi], "--") == 0) {
            argi++;
            break;
        } else {
            break;
        }
    }

    if (argi >= argc) {
        usage(argv[0]);
        return 1;
    }
    input_path = argv[argi++];
    if (argi < argc)
        output_path = argv[argi];

    /* read input */
    uint8_t *input_data;
    size_t input_len;

    if (strcmp(input_path, "-") == 0) {
        input_data = read_all(stdin, &input_len);
    } else {
        FILE *f = fopen(input_path, "rb");
        if (!f) {
            fprintf(stderr, "Error: cannot open input file '%s'\n", input_path);
            return 1;
        }
        input_data = read_all(f, &input_len);
        fclose(f);
    }

    if (!input_data) {
        fprintf(stderr, "Error: failed to read input\n");
        return 1;
    }

    /* decompress or compress */
    uint8_t *output_data = NULL;
    size_t output_len = 0;
    int rc;

    if (!do_compress) {
        /* guess output size: 10x input or 64KB, whichever is bigger */
        size_t out_cap = input_len * 10 + 65536;
        if (out_cap < 65536) out_cap = 65536;
        output_data = (uint8_t *)malloc(out_cap);
        if (!output_data) {
            fprintf(stderr, "Error: memory allocation failed\n");
            free(input_data);
            return 1;
        }

        rc = mozlz4_decompress(input_data, input_len, output_data, &output_len, out_cap);
        if (rc != MOZLZ4_OK) {
            const char *msg = "Unknown error";
            if (rc == MOZLZ4_ERR_MAGIC)      msg = "Unrecognized input file";
            if (rc == MOZLZ4_ERR_TOO_SHORT)  msg = "Input too short";
            if (rc == MOZLZ4_ERR_DECOMPRESS) msg = "Malformed input file";
            fprintf(stderr, "Error: %s\n", msg);
            free(input_data);
            free(output_data);
            return 1;
        }
    } else {
        size_t bound = mozlz4_compress_bound(input_len);
        output_data = (uint8_t *)malloc(bound);
        if (!output_data) {
            fprintf(stderr, "Error: memory allocation failed\n");
            free(input_data);
            return 1;
        }

        rc = mozlz4_compress(input_data, input_len, output_data, &output_len, bound);
        if (rc != MOZLZ4_OK) {
            fprintf(stderr, "Error: compression failed\n");
            free(input_data);
            free(output_data);
            return 1;
        }
    }

    free(input_data);

    /* write output */
    int write_rc;
    if (strcmp(output_path, "-") == 0) {
        write_rc = write_all(stdout, output_data, output_len);
    } else {
        FILE *f = fopen(output_path, "wb");
        if (!f) {
            fprintf(stderr, "Error: cannot open output file '%s'\n", output_path);
            free(output_data);
            return 1;
        }
        write_rc = write_all(f, output_data, output_len);
        fclose(f);
    }

    free(output_data);

    if (write_rc != 0) {
        fprintf(stderr, "Error: failed to write output\n");
        return 1;
    }

    return 0;
}
