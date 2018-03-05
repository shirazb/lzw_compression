#include <stddef.h>
#include <assert.h>
#include "lzw_decompressor.h"

/* To add a new type of error:
 *     1. Add it to `enum lzw_error` in the header.
 *     2. Increment `NUM_LZW_ERRORS`.
 *     3. Add the corresponding error message to the below array, keeping the
 *        messages in the same order as the errors in the enum.
 */
#define NUM_LZW_ERRORS 4
static char const *lzw_error_msgs[NUM_LZW_ERRORS] = {
        "Unknown error",
        "Okay",
        "Failed to open source file",
        "Failed to open destination file"
};

void lzw_init(
        struct lzw_decompressor *lzw,
        size_t code_length_bits,
        char *src_name,
        char *dst_name
) {
    assert(lzw);

    lzw->code_length_bits = code_length_bits;
    lzw->src_name = src_name;
    lzw->dst_name = dst_name;

    lzw->src = fopen(src_name, "rb");
    if (!lzw->src) {
        lzw->error = LZW_FAIL_OPEN_SRC;
        return;
    }

    lzw->dst = fopen(dst_name, "wb");
    if (!lzw->dst) {
        lzw->error = LZW_FAIL_OPEN_DST;
        return;
    }

    lzw->error = LZW_OKAY;
}

/**
 * Decompresses an LZW compressed file.
 * TODO: Document exact details from spec.
 * @param lzw Defines parameters of LZW decompressor.
 * @return true if successful, false otherwise.
 */
void lzw_decompress(struct lzw_decompressor *lzw) {
    assert(lzw);

    if (lzw->error != LZW_OKAY) {
        return;
    }
}

/**
 * Says whether or not a `struct lzw_decompressor` has an error.
 * @param lzw The decompressor in question.
 * @return true is there is an error, false otherwise.
 */
bool lzw_has_error(
        struct lzw_decompressor *lzw
) {
    assert(lzw);

    return lzw->error != LZW_OKAY;
}

/**
 * Converts an LZW decompressor error code to its corresponding string message.
 * @param error Error code.
 * @return Corresponding error message.
 */
const char *lzw_error_msg(struct lzw_decompressor *lzw) {
    assert(lzw);

    enum lzw_error error = lzw->error;
    // If error is unknown, set error to LZW_UNKNOWN_ERROR.
    if (!(0 <= error && error < NUM_LZW_ERRORS)) {
        error = LZW_UNKNOWN_ERROR;
    }

    return lzw_error_msgs[error];
}
