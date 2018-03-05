#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "lzw_dict.h"

#define NUM_ASCII_VALUES 256

static struct dict_entry {
    size_t size;
    uint8_t *bytes;
};

static bool ascii_table_initialised = false;
static uint8_t ascii_table[NUM_ASCII_VALUES];
static void initialise_ascii_table(void);

void dict_init(struct lzw_dict *dict, size_t code_length_bits) {
    /* Mallocing each byte of the initial entries individually is inefficient.
     * Also, it is wasteful as these entries are always the same thing for
     * every dictionary, so they can be shared. Thus, keep one global ASCII
     * table and initialise each dictionary's entries to point to it.
     */
    // FIXME: This is terrible. Ideally would use some kind of CPP for-loop to
    // generate in-line the ASCII entries.
    if (!ascii_table_initialised) {
        initialise_ascii_table();
        ascii_table_initialised = true;
    }

    assert(dict);

    // TODO: Protect against ridiculously large array.
    // Init `size` and `capacity`. Capacity is `2^(code_length_bits + 1) - 1`,
    // the number of items that can be represented by `code_length_bits` bits.
    dict->size = 0;
    dict->capacity = ((size_t) 1 << (code_length_bits + 1)) - 1;

    // TODO: Error when capacity < size required for ASCII.

    /* Malloc and initialise entries array. */

    dict->entries = malloc(sizeof(struct dict_entry) * dict->capacity);
    if (!dict->entries) {
        // TODO: Handle properly.
        return;
    }


    for (int i = 0; i < NUM_ASCII_VALUES; i++) {
        dict_add(dict, &ascii_table[i], 1);
    }
}

/**
 * Adds to the dictionary an entry of the given size and the given bytes.
 *
 * Will reset the dictionary if it is full.
 */
void dict_add(struct lzw_dict *dict, uint8_t *bytes, size_t size) {
    assert(dict);

    // TODO: Full dictionary.

    dict->entries[dict->size++] = { .size = size, .bytes = bytes };
}

static void initialise_ascii_table(void) {
    for (uint8_t i = 0; i < NUM_ASCII_VALUES; i++) {
        ascii_table[i] = i;
    }
}
