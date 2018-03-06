# lzw_compression
LZW compression of binary files in C.

Uses 12-bit fixed width codes. If odd number of codes, last two bytes form a padded 12-bit code.
Dictionary is initialised as an ASCII table.
If dictionary size exceeds 2^12 - 1, resets to ASCII table.

# Build
See `CMakeLists.txt`, should be simple cmake command.

# Usage
`./lzw_decompressor <src_file> <dst_file>`

# The LZW Decompressor Module

This module, defined in `src/lzw_decompressor.h`, provides a `struct lzw_decompressor` for performing decompression. It is used as follows:

```c
char *src_file_path = ...
char *dst_file_path = ...

struct lzw_decompressor lzw;
enum lzw_error error = lzw_init(&lzw, src_file_path, dst_file_path);

if (lzw_has_error(error) {
   // Handle
   char *err_msg = lzw_error_msg(error);
}

error = lzw_decompress(&lzw);

if (lzw_has_error(error) {
   // Handle
   char *err_msg = lzw_error_msg(error);
}

```

The `enum lzw_error` error returned is defined as follows:

```c
enum lzw_error {
    LZW_OKAY,
    LZW_UNKNOWN_ERROR,
    LZW_OPEN_SRC_ERROR,
    LZW_OPEN_DST_ERROR,
    LZW_HEAP_ERROR,
    LZW_WRITE_DST_ERROR,
    LZW_READ_ERROR,
    LZW_INVALID_FORMAT_ERROR,
};
```

`lzw_has_error(error)` returns `false` if error is `LZW_OKAY`, true otherwise.
