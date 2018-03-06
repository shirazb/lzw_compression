#ifndef LZW_COMPRESSION_LZW_H
#define LZW_COMPRESSION_LZW_H


#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "lzw_dict.h"

enum lzw_error {
    LZW_OKAY,
    LZW_UNKNOWN_ERROR,
    LZW_OPEN_SRC_ERROR,
    LZW_OPEN_DST_ERROR,
    LZW_HEAP_ERROR,
    LZW_WRITE_DST_ERROR,
    LZW_READ_ERROR,
    LZW_INVALID_FORMAT_ERROR,
};

struct lzw_decompressor {
    enum lzw_error error;      // Error code.
    FILE *src;                 // Source file.
    FILE *dst;                 // Destination file.
    struct lzw_dict dict;      // LZW dictionary used in decompression.

    /*
     * Used to read non-byte aligned amounts of data from the source file at
     * a time. See `read_next_code` in .c for more info.
     */
    uint8_t prev_bytes[2];     // Last two read bytes.
    bool odd;                  // If next code to be read is k^th code, true if
                               // k is odd, else false.
};

enum lzw_error lzw_init(
        struct lzw_decompressor *lzw,
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
