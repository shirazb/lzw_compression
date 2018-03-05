#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define REQUIRED_ARGC 3

static struct args {
    bool error;
    char *src_file;
    char *dst_file;
};

static void parse_args(struct args *args, int argc, char *argv[]);

int main(int argc, char *argv[]) {
    struct args args;
    parse_args(&args, argc, argv);

    if (args.error) {
        fprintf(stderr, "Usage: ./lzw_decompressor <src_file> <dst_file>");
        return EXIT_FAILURE;
    }

    printf("In main with valid args :)");
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
