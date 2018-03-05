#ifndef LZW_COMPRESSION_LZW_H
#define LZW_COMPRESSION_LZW_H


#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "lzw_dict.h"

enum lzw_error {
    LZW_UNKNOWN_ERROR,
    LZW_OKAY,
    LZW_FAIL_OPEN_SRC,
    LZW_FAIL_OPEN_DST,
    LZW_HEAP_ERROR
};

struct lzw_decompressor {
    enum lzw_error error;      // Error code.
    size_t code_length_bits;   // Length of codes in the source file in bits.
    FILE *src;                 // Source file.
    FILE *dst;                 // Destination file.
    struct lzw_dict dict;      // LZW dictionary used in decompression.
};

enum lzw_error lzw_init(
        struct lzw_decompressor *lzw,
        size_t code_length_bits,
        char *src_name,
        char *dst_name
);

void lzw_deinit(
        struct lzw_decompressor *lzw
);

enum lzw_error lzw_decompress(
        struct lzw_decompressor *lzw
);

bool lzw_has_error(
        enum lzw_error
);

const char *lzw_error_msg(
        enum lzw_error
);

#endif //LZW_COMPRESSION_LZW_H
