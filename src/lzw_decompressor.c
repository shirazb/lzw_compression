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
#define NUM_LZW_ERRORS 8
static char const *lzw_error_msgs[NUM_LZW_ERRORS] = {
        "Unknown error",
        "Okay",
        "Failed to open source file",
        "Failed to open destination file",
        "Heap error",
        "Failed to write to destination file",
        "Failed to read from the source file",
        "File is not in a valid LZW-encoded format"
};

/**
 * Generates code that will return with the error code if the given `struct
 * lzw_decompressor` contains an error.
 *
 * Evaluates `lzw` once and gets the error, to avoid reevaluating `lzw` which
 * may be a side-effecting expression.
 *
 * Declares this variable in a new scope to avoid name clashes.
 */
#define GUARD_ERROR(lzw)\
{\
    enum lzw_error __error = (lzw)->error; \
    if (lzw_has_error(__error)) { \
        return __error; \
    }\
}

static enum lzw_error append_byte_and_add_to_dict(
        struct lzw_decompressor *lzw,
        struct dict_entry *entry,
        uint8_t b,
        struct dict_entry **new_entry
);

static enum lzw_error write_next(
        struct lzw_decompressor *lzw,
        struct dict_entry *entry
);

static bool has_codes_remaining(struct lzw_decompressor *lzw);

static enum lzw_error read_and_lookup_next_code(
        struct lzw_decompressor *lzw,
        struct dict_entry **cur_entry
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
        lzw->error = LZW_OPEN_SRC_ERROR;
        return LZW_OPEN_SRC_ERROR;
    }

    lzw->dst = dst_name ? fopen(dst_name, "wb") : NULL;
    if (!lzw->dst) {
        lzw->error = LZW_OPEN_DST_ERROR;
        return LZW_OPEN_DST_ERROR;
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

    GUARD_ERROR(lzw);

    // Read the first code and look it up in the dictionary.
    struct dict_entry *cur_entry;
    lzw->error = read_and_lookup_next_code(lzw, &cur_entry);
    GUARD_ERROR(lzw);

    // First code should be in the dictionary, otherwise invalid encoding.
    if (!cur_entry) {
        lzw->error = LZW_INVALID_FORMAT_ERROR;
        return lzw->error;
    }

    // Write the first retrieved entry to the output.
    lzw->error = write_next(lzw, cur_entry);
    GUARD_ERROR(lzw);

    struct dict_entry *last_entry;

    // Keep decompressing until all codes in the input file have been consumed.
    while (has_codes_remaining(lzw)) {
        // Update last and cur entry.
        last_entry = cur_entry;
        lzw->error = read_and_lookup_next_code(lzw, &cur_entry);
        GUARD_ERROR(lzw);

        // If code is in the dictionary, write the current entry and add
        // <last entry><first byte of cur entry> to dictionary.
        if (cur_entry) {
            lzw->error = write_next(lzw, cur_entry);
            GUARD_ERROR(lzw);

            // FIXME: null ptr on cur_entry->bytes?
            struct dict_entry *new_entry;
            lzw->error = append_byte_and_add_to_dict(
                    lzw, last_entry, cur_entry->bytes[0], &new_entry
            );
            GUARD_ERROR(lzw);

        // If code is not in the dictionary, add <last entry><first byte of
        // last entry> to the dictionary, and write that to the output.
        } else {
            struct dict_entry *new_entry;
            lzw->error = append_byte_and_add_to_dict(
                    lzw, last_entry, last_entry->bytes[0], &new_entry
            );
            GUARD_ERROR(lzw);

            lzw->error = write_next(lzw, new_entry);
            GUARD_ERROR(lzw);
        }
    }

    assert(!lzw_has_error(lzw->error));
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

static enum lzw_error
write_next(struct lzw_decompressor *lzw, struct dict_entry *entry) {
    size_t written = fwrite(
            entry->bytes,
            sizeof(uint8_t),
            entry->size,
            lzw->dst
    );

    return entry->size == written ? LZW_OKAY : LZW_WRITE_DST_ERROR;
}

/**
 * Checks if there are codes still left to be read from the source file.
 */
static bool has_codes_remaining(struct lzw_decompressor *lzw) {
    assert(lzw);
    assert(!lzw_has_error(lzw->error));
    assert(lzw->src);

    return feof(lzw->src) == 0;
}

/**
 * Reads the next code from the source file, looks it up in the dictionary.
 * Sets `cur_entry` to be the returned entry (null if not present). Returns
 * an error code, erroring if the read failed. Note, the code not being in
 * the dictionary is ok, so does not result in an error.
 * @param lzw The decompressor.
 * @param cur_entry The entry found in the dictionary will be assigned to this.
 * @return LZW_READ_ERROR if error whilst reading, else LZW_OKAY.
 */
static enum lzw_error read_and_lookup_next_code(
        struct lzw_decompressor *lzw,
        struct dict_entry **cur_entry
) {
    assert(lzw);
    assert(!lzw_has_error(lzw->error));
    assert(lzw->src);
    assert(has_codes_remaining(lzw));

    // TODO: Read code.
    int code = 0;

    *cur_entry = dict_get(&lzw->dict, code);

    return LZW_OKAY;
}
