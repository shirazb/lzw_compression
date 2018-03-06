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

/**
 * Generates code that:
 * Takes a `uint8_t` called `param` and reads one byte from `lzw->src` into it.
 * Then, if read failed, returns false.
 */
#define read_byte_into_and_return_false_if_fail(param, lzw)\
{ \
    FILE *src = (lzw)->src; \
    size_t num_read = fread(&(param), sizeof(uint8_t), 1, src); \
    if (num_read != 1) return false; \
}

#define BYTE_IN_BITS 8
#define HALF_BYTE_IN_BITS 4

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

static bool read_next_code(struct lzw_decompressor *lzw, int *code);


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

    /* State for reading 12 bytes a time. */
    lzw->odd = true;
    lzw->last_byte = 0;

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
            assert(cur_entry->bytes);

            lzw->error = write_next(lzw, cur_entry);
            GUARD_ERROR(lzw);

            struct dict_entry *new_entry;
            lzw->error = append_byte_and_add_to_dict(
                    lzw, last_entry, cur_entry->bytes[0], &new_entry
            );
            GUARD_ERROR(lzw);

        // If code is not in the dictionary, add <last entry><first byte of
        // last entry> to the dictionary, and write that to the output.
        } else {
            assert(last_entry->bytes);

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
    assert(lzw);
    assert(!lzw_has_error(lzw->error));
    assert(lzw->dst);

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

    int code;
    bool read_success = read_next_code(lzw, &code);

    if (!read_success) {
        lzw->error = LZW_READ_ERROR;
        return LZW_READ_ERROR;
    }

    // Lookup code in dictionary
    *cur_entry = dict_get(&lzw->dict, code);

    return LZW_OKAY;
}

/**
 * Reads the next code from src file, storing it in the provided pointer.
 * Returns true if success, false otherwise.
 */
static bool read_next_code(struct lzw_decompressor *lzw, int *code) {
    /*
     * Need to read from the source file 12 bits a time, but can only read 8
     * bits a time. If on first call two bytes are read, do not want to discard
     * second byte and re-read it on second call as seeking backwards
     * through file is slow. Thus, store the last byte in the decompressor
     * struct.
     *
     * Two codes fits flush into three bytes (each are 24 bits). Thus, the
     * pattern for reading codes repeats every three bytes. That is:
     *     - On odd calls, we read two bytes. We want the first byte plus the
     *       first half of the second byte. We then cache the second byte in the
     *       struct.
     *     - On even calls, we read one more byte. We want the cached byte and
     *       the second half of the newly read byte.
     *
     * On odd calls, if after reading the two bytes the end of file is reached,
     * these two bytes become the code.
     */

    uint8_t byte0;
    uint8_t byte1;

    if (lzw->odd) {
        // Declare new `uint8_t`s and read the first and second bytes into them.
        read_byte_into_and_return_false_if_fail(byte0, lzw);
        read_byte_into_and_return_false_if_fail(byte1, lzw);

        if (has_codes_remaining(lzw)) {
            /*
             * If odd and not last code: Left shift the first byte by 4 to make
             * room for the remaining 4 bits of the code, taken from the top of
             * the second byte.
             */
            *code = (((int) byte0) << HALF_BYTE_IN_BITS) |
                    (int) (byte1 >> HALF_BYTE_IN_BITS);
        } else {
            /*
             * If odd and last code: Left shift the first byte by 8 to make
             * room for the entire second byte as the second half of the
             * 16-bit code.
             */
            *code = (((int) byte0) << BYTE_IN_BITS) | (int) byte1;
        }
    } else {
        /*
         * If even: Left shift the first byte by 4 to discard the first 4
         * bits. Then, extend and left shift by a further 4, so there is
         * now 8 bits on the right for the second byte.
         */
        byte0 = lzw->last_byte;
        read_byte_into_and_return_false_if_fail(byte1, lzw);
        *code = (((int) (byte0 << HALF_BYTE_IN_BITS)) << HALF_BYTE_IN_BITS) |
                (int) byte1;
    }

    lzw->last_byte = byte1;
    lzw->odd = !lzw->odd;

    return true;
}
