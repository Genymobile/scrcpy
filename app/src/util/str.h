#ifndef SC_STR_H
#define SC_STR_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>

/* Stringify a numeric value  */
#define SC_STR(s) SC_XSTR(s)
#define SC_XSTR(s) #s

/**
 * Like strncpy(), except:
 *  - it copies at most n-1 chars
 *  - the dest string is nul-terminated
 *  - it does not write useless bytes if strlen(src) < n
 *  - it returns the number of chars actually written (max n-1) if src has
 *    been copied completely, or n if src has been truncated
 */
size_t
sc_strncpy(char *dest, const char *src, size_t n);

/**
 * Join tokens by separator `sep` into `dst`
 *
 * Return the number of chars actually written (max n-1) if no truncation
 * occurred, or n if truncated.
 */
size_t
sc_str_join(char *dst, const char *const tokens[], char sep, size_t n);

/**
 * Quote a string
 *
 * Return a new allocated string, surrounded with quotes (`"`).
 */
char *
sc_str_quote(const char *src);

/**
 * Parse `s` as an integer into `out`
 *
 * Return true if the conversion succeeded, false otherwise.
 */
bool
sc_str_parse_integer(const char *s, long *out);

/**
 * Parse `s` as integers separated by `sep` (for example `1234:2000`) into `out`
 *
 * Returns the number of integers on success, 0 on failure.
 */
size_t
sc_str_parse_integers(const char *s, const char sep, size_t max_items,
                      long *out);

/**
 * Parse `s` as an integer into `out`
 *
 * Like `sc_str_parse_integer()`, but accept 'k'/'K' (x1000) and 'm'/'M'
 * (x1000000) as suffixes.
 *
 * Return true if the conversion succeeded, false otherwise.
 */
bool
sc_str_parse_integer_with_suffix(const char *s, long *out);

/**
 * Search `s` in the list separated by `sep`
 *
 * For example, sc_str_list_contains("a,bc,def", ',', "bc") returns true.
 */
bool
sc_str_list_contains(const char *list, char sep, const char *s);

/**
 * Return the index to truncate a UTF-8 string at a valid position
 */
size_t
sc_str_utf8_truncation_index(const char *utf8, size_t max_len);

#ifdef _WIN32
/**
 * Convert a UTF-8 string to a wchar_t string
 *
 * Return the new allocated string, to be freed by the caller.
 */
wchar_t *
sc_str_to_wchars(const char *utf8);

/**
 * Convert a wchar_t string to a UTF-8 string
 *
 * Return the new allocated string, to be freed by the caller.
 */
char *
sc_str_from_wchars(const wchar_t *s);
#endif

/**
 * Wrap input lines to fit in `columns` columns
 *
 * Break input lines at word boundaries (spaces) so that they fit in `columns`
 * columns, left-indented by `indent` spaces.
 */
char *
sc_str_wrap_lines(const char *input, unsigned columns, unsigned indent);

/**
 * Find the start of a column in a string
 *
 * A string may represent several columns, separated by some "spaces"
 * (separators). This function aims to find the start of the column number
 * `col`.
 *
 * For example, to find the 4th column (column number 3):
 *
 *     //                               here
 *     //                               v
 *     const char *s = "abc def    ghi  jk";
 *     ssize_t index = sc_str_index_of_column(s, 3, " ");
 *     assert(index == 16); // points to "jk"
 *
 * Return -1 if no such column exists.
 */
ssize_t
sc_str_index_of_column(const char *s, unsigned col, const char *seps);

/**
 * Remove all `\r` at the end of the line
 *
 * The line length is provided by `len` (this avoids a call to `strlen()` when
 * the caller already knows the length).
 *
 * Return the new length.
 */
size_t
sc_str_remove_trailing_cr(char *s, size_t len);

/**
 * Convert binary data to hexadecimal string
 */
char *
sc_str_to_hex_string(const uint8_t *data, size_t len);

#endif
