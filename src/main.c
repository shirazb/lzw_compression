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

    /* Perform decompression. */

    struct lzw_decompressor lzw;
    enum lzw_error error = lzw_init(
            &lzw,
            CODE_LENGTH_BITS,
            args.src_file,
            args.dst_file
    );

    if (lzw_has_error(error)) {
        fprintf(stderr, "ERROR: %s.\n", lzw_error_msg(error));
        return EXIT_FAILURE;
    }

    error = lzw_decompress(&lzw);

    int exit_code;
    if (lzw_has_error(error)) {
        fprintf(stderr, "ERROR: %s.\n", lzw_error_msg(error));
        exit_code = EXIT_FAILURE;
    } else {
        exit_code = EXIT_SUCCESS;
    }

    lzw_deinit(&lzw);

    return exit_code;
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
