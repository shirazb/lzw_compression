#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "lzw_dict.h"

#define NUM_ASCII_VALUES 256

static struct dict_entry {
    size_t size;
    uint8_t *bytes;
};

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
 * @param code_length_bits The length of codes in the source file, which will
 * determine the capacity of the dictionary.
 */
void dict_init(struct lzw_dict *dict, size_t code_length_bits) {
    // FIXME: This is terrible. Ideally would use some kind of CPP for-loop to
    // generate in-line the ASCII entries.
    if (!ascii_table_initialised) {
        initialise_ascii_table();
        ascii_table_initialised = true;
    }

    assert(dict);

    // TODO: Protect against ridiculously large array.
    // Init `size` and `capacity`. Capacity is `2^code_length_bits - 1`,
    // the number of items that can be represented by `code_length_bits` bits.
    dict->size = 0;
    dict->capacity = (size_t) 1 << (code_length_bits);

    // TODO: Handle when capacity < size required for ASCII?

    /* Malloc and initialise entries array. */

    // Allocate entire array now as not actually that big. Although capacity
    // varies exponentially with `code_length_bits`, check before that
    // capacity is not too large.
    dict->entries = malloc(sizeof(struct dict_entry) * dict->capacity);
    if (!dict->entries) {
        // TODO: Handle properly.
        return;
    }

    // Initialise dictionary with ASCII table.
    for (int i = 0; i < NUM_ASCII_VALUES; i++) {
        dict_add(dict, &ascii_table[i], 1);
    }
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
 * Adds to the dictionary an entry of the given size and the given bytes.
 *
 * Will reset the dictionary if it is full.
 */
void dict_add(struct lzw_dict *dict, uint8_t *bytes, size_t size) {
    assert(dict);

    // If too big, reset dictionary.
    if ((size_t) dict->size >= dict->capacity) {
        reset_dict(dict);
    }

    // Set size and bytes and increment dictionary size.
    int i = dict->size;
    dict->entries[i].size = size;
    dict->entries[i].bytes = bytes;
    dict->size += 1;
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
    dict->size = NUM_ASCII_VALUES;
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
    // Stop at `dict->size` as the rest should be unused. If previously used,
    // it should have already been freed.
    for (int i = NUM_ASCII_VALUES; i < dict->size; i++) {
        free(dict->entries[i].bytes);
    }
}
