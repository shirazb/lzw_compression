#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lzw_dict.h"

#define NUM_ASCII_VALUES 256
#define CODE_WIDTH_BITS 12

/* Mallocing each byte of the initial entries individually is inefficient.
 * Also, it is wasteful as these entries are always the same thing for
 * every dictionary, so they can be shared. Thus, keep one global ASCII
 * table and initialise each dictionary's entries to point to it.
 */
static bool ascii_table_initialised = false;
static uint8_t ascii_table[NUM_ASCII_VALUES];
static void initialise_ascii_table(void);

static void reset_dict(struct lzw_dict *dict);
static void deinit_dict_entries(struct lzw_dict *dict);

/**
 * Initialises an LZW dictionary.
 * @param dict The `struct lzw_dict` to initialise.
 * determine the capacity of the dictionary.
 * @return true if initialisation successful, false otherwise.
 */
bool dict_init(struct lzw_dict *dict) {
    // FIXME: This is terrible. Ideally would use some kind of CPP for-loop to
    // generate in-line the ASCII entries at the declaration.
    if (!ascii_table_initialised) {
        initialise_ascii_table();
        ascii_table_initialised = true;
    }

    assert(dict);

    // TODO: Protect against ridiculously large array.
    // Init `size` and `capacity`. Capacity is `2^code_length_bits - 1`,
    // the number of items that can be represented by `code_length_bits` bits.
    dict->next_idx = 0;
    dict->capacity = (size_t) 1 << CODE_WIDTH_BITS;

    // TODO: Handle when capacity < size required for ASCII?

    /* Malloc and initialise entries array. */

    // Allocate entire array now as not actually that big. Although capacity
    // varies exponentially with `code_length_bits`, check before that
    // capacity is not too large.
    dict->entries = malloc(sizeof(struct dict_entry) * dict->capacity);
    if (!dict->entries) {
        return false;
    }

    // Initialise dictionary with ASCII table.
    for (int i = 0; i < NUM_ASCII_VALUES; i++) {
        dict_add(dict, &ascii_table[i], 1);
    }

    return true;
}

/**
 * Cleans up.
 */
void dict_deinit(struct lzw_dict *dict) {
    assert(dict);

    deinit_dict_entries(dict);

    // Free the entries array.
    free(dict->entries);
}

/**
 * Checks if the code is in the dictionary.
 */
bool dict_contains(struct lzw_dict *dict, int code) {
    assert(dict);

    // Equivalent to checking if the code is below the current size.
    return code < dict->next_idx;
}

/**
 * Adds to the dictionary an entry of the given size and the given bytes.
 *
 * Will reset the dictionary if it is full.
 */
struct dict_entry *dict_add(
        struct lzw_dict *dict,
        uint8_t *bytes,
        size_t size
) {
    assert(dict);

    // If too big, reset dictionary.
    if ((size_t) dict->next_idx >= dict->capacity) {
        reset_dict(dict);
    }

    // Set size and bytes and increment dictionary next index.
    struct dict_entry *new_entry = &dict->entries[dict->next_idx];
    new_entry->size = size;
    new_entry->bytes = bytes;

    dict->next_idx += 1;
    return new_entry;
}

/**
 * Gets the `struct dict_entry` at `code` in the dictionary `dict`. Returns
 * null if not present.
 */
struct dict_entry *dict_get(
        struct lzw_dict *dict,
        int code
) {
    assert(dict);

    return dict_contains(dict, code) ? &dict->entries[code] : NULL;
}

static void initialise_ascii_table(void) {
    for (int i = 0; i < NUM_ASCII_VALUES; i++) {
        ascii_table[i] = (uint8_t) i;
    }
}

/**
 * Resets the dictionary so that it only contains the ASCII values as the
 * first 256 entries.
 */
static void reset_dict(struct lzw_dict *dict) {
    assert(dict);

    deinit_dict_entries(dict);
    dict->next_idx = NUM_ASCII_VALUES;
}

/**
 * Frees the memory pointed to from inside of the entries (the bytes fields, not
 * the entries themselves).
 */
static void deinit_dict_entries(struct lzw_dict *dict) {
    assert(dict);
    assert(dict->entries);

    // Free the `bytes` fields of each `dict_entry` as these were malloc'd.
    // Start at NUM_ASCII_VALUES as those before point to the statically
    // allocated ASCII table array.
    // Stop at `dict->next_idx` as the rest should be unused; not allocated.
    for (int i = NUM_ASCII_VALUES; i < dict->next_idx; i++) {
        free(dict->entries[i].bytes);
    }
}
