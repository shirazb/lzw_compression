#ifndef LZW_COMPRESSION_LZW_H
#define LZW_COMPRESSION_LZW_H


#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

enum lzw_error {
    LZW_UNKNOWN_ERROR,
    LZW_OKAY,
    LZW_FAIL_OPEN_SRC,
    LZW_FAIL_OPEN_DST
};

struct lzw_decompressor {
    enum lzw_error error;
    size_t code_length_bits;
    char *src_name;
    char *dst_name;
    FILE *src;
    FILE *dst;
};

void lzw_init(
        struct lzw_decompressor *lzw,
        size_t code_length_bits,
        char *src_name,
        char *dst_name
);

void lzw_decompress(
        struct lzw_decompressor *lzw
);

bool lzw_has_error(
        struct lzw_decompressor *lzw
);

const char *lzw_error_msg(
        struct lzw_decompressor *lzw
);

#endif //LZW_COMPRESSION_LZW_H
