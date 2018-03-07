#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lzw_decompressor.h"


/**************************   Prototypes   ************************************/


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

static struct dict_entry *lookup_code(struct lzw_decompressor *lzw, int code);

static bool read_next_code(struct lzw_decompressor *lzw, int *code);


/****************************   Macros   **************************************/


/* To add a new type of error:
 *     1. Add it to `enum lzw_error` in the header.
 *     2. Increment `NUM_LZW_ERRORS`.
 *     3. Add the corresponding error message to the below array, keeping the
 *        messages in the same order as the errors in the enum.
 */
#define NUM_LZW_ERRORS 8
static char const *lzw_error_msgs[NUM_LZW_ERRORS] = {
        "Okay",
        "Unknown error",
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
#define GUARD_ANY(lzw)\
{\
    enum lzw_error __error = (lzw)->error; \
    if (lzw_has_error(__error)) { \
        return __error; \
    }\
}

/**
 * Generates code that: if `cond` will set `lzw->error` to `error` and return
 * `error.
 */
#define GUARD(cond, e, lzw)\
{ \
    struct lzw_decompressor *__l = lzw; \
    enum lzw_error __e = e; \
    if (cond) { \
        __l->error = __e; \
        return __e; \
    } \
}

/* Used when reading bytes. See `read_next_code`. */
#define BYTE_IN_BITS 8
#define HALF_BYTE_IN_BITS 4
#define CLEAR_FIRST_HALF 0x0F

#ifndef NDEBUG
/**
 * Used for debugging, prints the bit pattern of the given byte and a new line.
 * @param byte
 */
void print_bin(uint8_t byte)
{
    int i = BYTE_IN_BITS;
    while (i--) {
        putchar('0' + ((byte >> i) & 1));
    }
    putchar('\n');
}
#else
#define print_bin(X) (void)0;
#endif


/****************************   Public API   **********************************/


/**
 * Initialises a new LZW decompressor. Takes input from a binary file and
 * writes decompressed output to a binary file.
 * @param lzw The lzw_decompressor to initialise.
 * @param code_length_bits The length in bits of the codes in the source.
 * @param src_name Path to the source file.
 * @param dst_name Path to the destination file.
 * @return LZW_OKAY if no error, otherwise the error encountered.
 */
enum lzw_error lzw_init(
        struct lzw_decompressor *lzw,
        char *src_name,
        char *dst_name
) {
    assert(lzw);

    /* Open source and destination files. */

    lzw->src = src_name ? fopen(src_name, "rb") : NULL;
    GUARD(!lzw->src, LZW_OPEN_SRC_ERROR, lzw);

    lzw->dst = dst_name ? fopen(dst_name, "wb") : NULL;
    GUARD(!lzw->dst, LZW_OPEN_DST_ERROR, lzw);

    /* Initialise dictionary. */
    bool dict_init_success = dict_init(&lzw->dict);
    // TODO: Implement proper dictionary errors. Right now, only can
    // error due to failed malloc.
    GUARD(!dict_init_success, LZW_HEAP_ERROR, lzw);

    /* Initialise odd to true, as next byte to be read is the first. */
    lzw->odd = true;

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
 * @param lzw The initialised LZW decompressor.
 * @return LZW_OKAY if successful, otherwise the error encountered.
 */
enum lzw_error lzw_decompress(struct lzw_decompressor *lzw) {
    assert(lzw);

    GUARD_ANY(lzw);

    // Read the first code and look it up in the dictionary.
    int cur_code;
    struct dict_entry *cur_entry;

    read_next_code(lzw, &cur_code);
    GUARD_ANY(lzw);

    cur_entry = lookup_code(lzw, cur_code);

    // First code should be in the dictionary, otherwise invalid encoding.
    GUARD(!cur_entry, LZW_INVALID_FORMAT_ERROR, lzw);

    // Write the first retrieved entry to the output.
    lzw->error = write_next(lzw, cur_entry);
    GUARD_ANY(lzw);

    int last_code = cur_code;
    struct dict_entry *last_entry;

    // Keep decompressing until all codes in the input file have been consumed.
    while (read_next_code(lzw, &cur_code)) {
        // Update cur code, cur entry, and last entry. Last code updated at
        // end of loop.
        assert(!lzw_has_error(lzw->error));
        cur_entry = lookup_code(lzw, cur_code);
        last_entry = lookup_code(lzw, last_code);

        assert(last_entry);

        // If code is in the dictionary, write the current entry and add
        // <last entry><first byte of cur entry> to dictionary. Update last.
        if (cur_entry) {
            assert(cur_entry->bytes);

            lzw->error = write_next(lzw, cur_entry);
            GUARD_ANY(lzw);

            struct dict_entry *new_entry;
            lzw->error = append_byte_and_add_to_dict(
                    lzw, last_entry, cur_entry->bytes[0], &new_entry
            );
            GUARD_ANY(lzw);

            last_code = cur_code;

        // If code is not in the dictionary, add <last entry><first byte of
        // last entry> to the dictionary, and write that to the output.
        } else {
            assert(last_entry->bytes);

            struct dict_entry *new_entry;
            lzw->error = append_byte_and_add_to_dict(
                    lzw, last_entry, last_entry->bytes[0], &new_entry
            );
            GUARD_ANY(lzw);

            lzw->error = write_next(lzw, new_entry);
            GUARD_ANY(lzw);
        }

    }

    // Could have been a read error.
    GUARD_ANY(lzw);

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


/*****************************   Helpers   ************************************/


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
    assert(lzw);
    assert(!lzw_has_error(lzw->error));

    size_t entry_size = sizeof(uint8_t) * entry->size;
    size_t new_entry_size = sizeof(uint8_t) * (entry->size + 1);

    // Allocate memory to copy into, of entry's size plus 1 more byte.
    uint8_t *new_bytes = malloc(new_entry_size);
    if (!new_bytes) {
        return LZW_HEAP_ERROR;
    }

    // Copy entry and extra byte into new memory.
    memcpy(new_bytes, entry->bytes, entry_size);
    new_bytes[entry->size] = b;

    // Add new bytes to dictionary and assign to passed in new_entry pointer.
    *new_entry = dict_add(&lzw->dict, new_bytes, entry->size + 1);

    return LZW_OKAY;
}

static enum lzw_error write_next(
        struct lzw_decompressor *lzw,
        struct dict_entry *entry
) {
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
 * Looks up the given code in the dictionary.
 *
 * TODO: When are codes invalid? If possible, refactor to return error.
 * @param lzw The decompressor whose dictionary the code will be looked up in.
 * @param code The code to look up.
 * @return The `struct dict_entry *` found in the dictionary. NULL if not found.
 */
static struct dict_entry *lookup_code(struct lzw_decompressor *lzw, int code) {
    assert(lzw);
    assert(!lzw_has_error(lzw->error));

    return dict_get(&lzw->dict, code);
}

/**
 * Reads the next code from the source file, storing it in the provided pointer.
 * Returns true if codes were read, false otherwise (EOF reached). If there
 * was some kind of read error, sets `lzw->error` to `LZW_READ_ERROR` and
 * returns false.
 */
bool read_next_code(struct lzw_decompressor *lzw, int *code) {
    /*
     * Need to take codes from the source file 12 bits a time, but can only
     * read 8 bits a time. Two codes fits flush into three bytes (each are 24
     * bits):
     *
     * b7       .....      b0
     * b15 ... b12 b11 ... b8
     * b23      .....      b16
     *
     * <b7 - b0><b15 - b12> is the first code.
     * <b11 - b8><b23 - b16> is the second code.
     * The first code starts aligned with the next byte.
     *
     * Thus, the pattern for reading codes repeats every three bytes. Thus:
     *     - On odd calls, we read three bytes. We want the first byte plus the
     *       first half of the second byte. We then cache the second and
     *       third bytes in the decompressor struct.
     *     - On even calls, we use the bytes cached in the struct. We want
     *       the second half of the first byte plus the second byte.
     *
     * We also need to handle reaching the EOF. If on an odd call, no bytes
     * at all can be read, there was an even number of codes, so the last
     * call dealt with the last code. If two bytes can be read but not the
     * third, there is an odd number of codes, so the two read bytes become
     * the padded 16-bit code.
     */
    assert(lzw);
    assert(!lzw_has_error(lzw->error));

    uint8_t data[3];

    if (lzw->odd) {
        // Try to read two bytes.
        size_t n = fread(data, sizeof(uint8_t), 2, lzw->src);

        // If could not read any bytes: EOF and even number of codes. Last
        // call dealt with them.
        if (n == 0) {
            return false;
        }

        // If only read 1 byte: This should not happen, set error.
        if (n == 1) {
            lzw->error = LZW_READ_ERROR;
            return false;
        }

        assert(n == 2);

        // Have successfully read two bytes, try to read a third byte.
        n += fread(&data[2], sizeof(uint8_t), 1, lzw->src);

        /*
         * If code not read a third byte: EOF and odd number of codes.
         *
         * Left shift the first byte by 8 to make room for the entire second
         * byte as the second half of the 16-bit code.
         */
        if (n == 2) {
            *code = (((int) data[0]) << BYTE_IN_BITS) | (int) data[1];

        /*
         * If successfully read all three bytes: not EOF and odd number of
         * codes.
         *
         * Left shift the first byte by 4 to make room for the remaining 4
         * bits of the code, taken from the top half of the second byte.
         *
         * Cache the second two bytes so they can be used on the next (even)
         * call.
         */
        } else {
            assert(n == 3);
            *code = (((int) data[0]) << HALF_BYTE_IN_BITS) |
                    (int) (data[1] >> HALF_BYTE_IN_BITS);

            lzw->prev_bytes[0] = data[1];
            lzw->prev_bytes[1] = data[2];
        }

    /*
     * If even.
     *
     * Use the two bytes already read in the last (odd) call.
     *
     * Clear the first four bits of the first byte, then left shift by 8 to
     * make room for the second byte.
     */
    } else {
        *code = ((lzw->prev_bytes[0] & CLEAR_FIRST_HALF) << BYTE_IN_BITS) |
                (int) lzw->prev_bytes[1];
    }

    lzw->odd = !lzw->odd;
    return true;
}
