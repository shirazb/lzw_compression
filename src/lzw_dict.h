#ifndef LZW_COMPRESSION_DICTIONARY_H
#define LZW_COMPRESSION_DICTIONARY_H

#include <stddef.h>

static struct dict_entry;

struct lzw_dict {
    size_t size;
    size_t capacity;
    struct dict_entry *entries;
};

void dict_init(struct lzw_dict *dict, size_t code_length_bits);
void dict_add(struct lzw_dict *dict, uint8_t *bytes, size_t size);

#endif //LZW_COMPRESSION_DICTIONARY_H
