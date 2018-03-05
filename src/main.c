#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include "lzw_decompressor.h"

#define REQUIRED_ARGC 3
#define CODE_LENGTH_BITS 12

static struct args {
    bool error;
    char *src_file;
    char *dst_file;
};

static void parse_args(struct args *args, int argc, char *argv[]);

int main(int argc, char *argv[]) {
    // Parse arguments.
    struct args args;
    parse_args(&args, argc, argv);

    // If invalid args, print usage msg and fail with error.
    if (args.error) {
        fprintf(stderr, "Usage: ./lzw_decompressor <src_file> <dst_file>");
        return EXIT_FAILURE;
    }

    // Perform decompression.
    struct lzw_decompressor lzw;
    lzw_init(&lzw, CODE_LENGTH_BITS, args.src_file, args.dst_file);
    lzw_decompress(&lzw);

    // Print if error and exit with failure.
    if (lzw_has_error(&lzw)) {
        fprintf(stderr, "ERROR: %s.\n", lzw_error_msg(&lzw));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
 * Parses the arguments of the program.
 * Checks correct number of args and args themselves are valid.
 * If ok, args->error is false, true otherwise.
 */
static void parse_args(struct args *args, int argc, char *argv[]) {
    assert(args);

    if (argc != REQUIRED_ARGC) {
        args->error = true;
        return;
    }

    // TODO: Check valid
    args->src_file = argv[1];
    args->dst_file = argv[2];
    args->error = false;
}
