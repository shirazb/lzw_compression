#ifndef LZW_COMPRESSION_DICTIONARY_H
#define LZW_COMPRESSION_DICTIONARY_H

#include <stddef.h>
#include <stdint.h>

struct dict_entry {
    size_t size;
    uint8_t *bytes;
};

struct lzw_dict {
    /* Current size. Used as index of next entry, so keep as int not size_t.
       When comparing to capacity, cast this to size_t, not other way around,
       because int definitely fits in size_t. */
    int size;

    /* Capacity of array based on length of codes. */
    size_t capacity;

    /* Array of entries.
     *
     * Using an array with each element pointing to its own string provides fast
     * lookup.
     *
     * Using the indexes to index into a table of linked lists, where each
     * node points to an entry in a shared ASCII table, would save memory but
     * be slower. */
    struct dict_entry *entries;
};

void dict_init(
        struct lzw_dict *dict,
        size_t code_length_bits
);

bool dict_contains(
        struct lzw_dict *dict,
        int code
);

struct dict_entry *dict_add(
        struct lzw_dict *dict,
        uint8_t *bytes,
        size_t size
);

void dict_deinit(
        struct lzw_dict *dict
);

#endif //LZW_COMPRESSION_DICTIONARY_H
