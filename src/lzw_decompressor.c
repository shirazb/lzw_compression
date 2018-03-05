#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lzw_decompressor.h"

/* To add a new type of error:
 *     1. Add it to `enum lzw_error` in the header.
 *     2. Increment `NUM_LZW_ERRORS`.
 *     3. Add the corresponding error message to the below array, keeping the
 *        messages in the same order as the errors in the enum.
 */
#define NUM_LZW_ERRORS 5
static char const *lzw_error_msgs[NUM_LZW_ERRORS] = {
        "Unknown error",
        "Okay",
        "Failed to open source file",
        "Failed to open destination file",
        "Heap error"
};

static enum lzw_error append_byte_and_add_to_dict(
        struct lzw_decompressor *lzw,
        struct dict_entry *entry,
        uint8_t b,
        struct dict_entry **new_entry
);

/**
 * Initialises a new LZW decompressor. Takes input from a binary file and
 * writes decompressed output to a binary file.
 * @param lzw The lzw_decompressor to initialise.
 * @param code_length_bits The length in bits of the codes in the source.
 * @param src_name Path to the source file.
 * @param dst_name Path to the destination file.
 */
enum lzw_error lzw_init(
        struct lzw_decompressor *lzw,
        size_t code_length_bits,
        char *src_name,
        char *dst_name
) {
    assert(lzw);

    /* Code length. */

    lzw->code_length_bits = code_length_bits;

    /* Open source and destination files. */

    lzw->src = src_name ? fopen(src_name, "rb") : NULL;
    if (!lzw->src) {
        lzw->error = LZW_FAIL_OPEN_SRC;
        return LZW_FAIL_OPEN_SRC;
    }

    lzw->dst = dst_name ? fopen(dst_name, "wb") : NULL;
    if (!lzw->dst) {
        lzw->error = LZW_FAIL_OPEN_DST;
        return LZW_FAIL_OPEN_DST;
    }

    /* Initialise dictionary. */
    dict_init(&lzw->dict, lzw->code_length_bits);

    lzw->error = LZW_OKAY;
    return LZW_OKAY;
}

/**
 * Cleans up decompressor.
 */
void lzw_deinit(struct lzw_decompressor *lzw) {
    assert(lzw);

    /* Close files if opened. */

    assert(lzw->src);
    fclose(lzw->src);

    assert(lzw->dst);
    fclose(lzw->dst);

    /* De-initialise the dictionary. */
    dict_deinit(&lzw->dict);
}

/**
 * Decompresses an LZW compressed file.
 * TODO: Document exact details from spec.
 *
 * Will de-initialise the `lzw_decompressor` internally (free memory, close
 * files, etc).
 * @param lzw Defines parameters of LZW decompressor.
 * @return true if successful, false otherwise.
 */
enum lzw_error lzw_decompress(struct lzw_decompressor *lzw) {
    assert(lzw);

    if (lzw->error != LZW_OKAY) {
        return lzw->error;
    }

    // Write first code to output.
    struct dict_entry *cur_entry = read_and_lookup_next_code(lzw);
    write_next(lzw, cur_entry);

    struct dict_entry *last_entry;

    while (has_codes_remaining(lzw)) {
        last_entry = cur_entry;
        cur_entry = read_and_lookup_next_code(lzw);

        if (cur_entry) {
            write_next(lzw, cur_entry);
            // FIXME: null ptr on cur_entry->bytes?
            struct dict_entry *new_entry;
            enum lzw_error error = append_byte_and_add_to_dict(
                    lzw, last_entry, cur_entry->bytes[0], &new_entry
            );

            if (error) {
                lzw->error = error;
                return error;
            }
        } else {
            struct dict_entry *new_entry;
            enum lzw_error error = append_byte_and_add_to_dict(
                    lzw, last_entry, last_entry->bytes[0], &new_entry
            );

            if (error) {
                lzw->error = error;
                return error;
            }

            write_next(lzw, new_entry);
        }
    }

    assert(lzw->error == LZW_OKAY);
    return lzw->error;
}

/**
 * Says whether or not a `struct lzw_decompressor` has an error.
 * @param lzw The decompressor in question.
 * @return true if there is an error, false otherwise.
 */
bool lzw_has_error(enum lzw_error error) {
    return error != LZW_OKAY;
}

/**
 * Converts an LZW decompressor error code to its corresponding string message.
 * @param error Error code.
 * @return Corresponding error message.
 */
const char *lzw_error_msg(enum lzw_error error) {
    // If error is unknown, set error to LZW_UNKNOWN_ERROR.
    if (!(0 <= error && error < NUM_LZW_ERRORS)) {
        error = LZW_UNKNOWN_ERROR;
    }

    return lzw_error_msgs[error];
}

/**
 * In newly allocated memory, copies over the given entry plus the given byte
 * appended at the end, then inserts this into the dictionary. Places the
 * newly created entry into `new_entry`. Returns an LZW error code.
 * @param lzw The `struct lzw_decompressor` containing the dictionary.
 * @param entry The dictionary entry to append to.
 * @param b The byte to append.
 * @param new_entry Blank entry passed in that will be set to the newly
 * created entry.
 * @return `enum lzw_error` error code.
 */
static enum lzw_error append_byte_and_add_to_dict(
        struct lzw_decompressor *lzw,
        struct dict_entry *entry,
        uint8_t b,
        struct dict_entry **new_entry
) {
    size_t entry_size = sizeof(uint8_t) * entry->size;
    size_t new_entry_size = sizeof(uint8_t) * (entry->size + 1);

    // Allocate memory to copy into, of entry's size plus 1 more byte.
    uint8_t *new_bytes = malloc(new_entry_size);
    if (!new_bytes) {
        return LZW_HEAP_ERROR;
    }

    // Copy entry and extra byte into new memory.
    memcpy(new_bytes, entry->bytes, entry_size);
    new_bytes[entry->size + 1] = b;

    // Add new bytes to dictionary and assign to passed in new_entry pointer.
    *new_entry = dict_add(&lzw->dict, new_bytes, entry->size + 1);

    return LZW_OKAY;
}
