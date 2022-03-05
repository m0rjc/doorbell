#pragma once

#include <stdlib.h>

typedef struct {
    /** Start of the string */
    char *buffer;
    /** Current pointer */
    char *ptr;
    /** Amount remaining */
    int remaining;
    /** Requested additional buffer space */
    void *additional;
} string_builder_t;

string_builder_t *sb_new(size_t len);

/**
 * @brief return an anrray of count string builders with the given list of buffer sizes.
 * The buffer is a single malloc, so free it to free all allocated space.
 * 
 * @param count number of stringbuilders
 * @param ... list of sizes for the buffers/
 * @return string_builder_t* array of stringbuilders.
 */
string_builder_t *sb_new_multiple(int count, ...);

/**
 * @brief Append src to dest if it fits within len.
 * Leave dest pointing to the null terminator.
 * 
 * @param this string builder structure
 * @param src source string null terminated
 */
void sb_append(string_builder_t *sb, const char *src);

/**
 * @brief HTML escape an input string
 * 
 * @param dest destination pointer, mutable
 * @param len reminaing available space
 * @param src source string null terminated
 * @return int new remaining space for appending overwriting the terminator, or 0 if failed.
 */
void sb_append_htmlescape(string_builder_t *sb, const char *input);

size_t sb_predict_escaped_length(const char *input);

void sb_appendf(string_builder_t *sb, const char *format, ...);

#define sb_isok(sb) ((sb)->remaining > 0)