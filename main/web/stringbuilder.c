#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include "stringbuilder.h"

string_builder_t *sb_new_multiple(int count, ...){
    va_list args;

    size_t header_len = count * sizeof(string_builder_t);
    size_t data_len = 0;
    va_start(args, count);
    for(int i = 0; i < count; i++) {
        size_t size = va_arg(args, size_t);
        data_len += size;
    }
    va_end(args);

    string_builder_t *buffer = malloc(header_len + data_len);
    if(buffer == NULL) return NULL;

    char *heap = (char *)(buffer + count); // Rely on pointer arithmetic to do the addition in string_builder_t 

    va_start(args, count);
    for(int i = 0; i < count; i++) {
        size_t size = va_arg(args, size_t);
        buffer[i].buffer = buffer[i].ptr = heap;
        buffer[i].remaining = size;
        heap += size;
    }
    va_end(args);
    
    return buffer;
}

string_builder_t *sb_new(size_t len) {
    string_builder_t *buffer = malloc(len + sizeof(string_builder_t));
    string_builder_t *start_of_free = buffer + 1;
    buffer->ptr = buffer->buffer = (char *)start_of_free;
    buffer->remaining = len;
    return buffer;
}

/**
 * @brief Append src to dest if it fits within len.
 * Leave dest pointing to the null terminator.
 * 
 * @param sb string builder structure
 * @param src source string null terminated
 */
void sb_append(string_builder_t *sb, const char *src) {
    int src_len = strlen(src);
    if(src_len >= (sb->remaining)-1) {
        sb->remaining = -1;
        return;
    }
    memcpy(sb->ptr, src, src_len+1);
    sb->ptr += src_len;
    sb->remaining -= src_len;
}

/**
 * @brief HTML escape an input string
 * 
 * @param dest destination pointer, mutable
 * @param len reminaing available space
 * @param src source string null terminated
 * @return int new remaining space for appending overwriting the terminator, or 0 if failed.
 */
void sb_append_htmlescape(string_builder_t *sb, const char *input) {
    int done = 0;
    int local_remaining = sb->remaining;
    for(const char *src = input; !done && local_remaining > 0; src++) {
        switch (*src) {
            case '&':
                sb_append(sb, "&amp;");
                break;
            case '"':
                sb_append(sb, "&quot;");
                break;
            case '<':
                sb_append(sb, "&lt;");
                break;
            case '>':
                sb_append(sb, "&gt;");
                break;
            case 0:
                // Leave dest pointing at the null terminator so we can continue.
                // Remaining is left ready for that continuation
                done = 1;
                *(sb->ptr) = 0;
                break;
            default:
                *(sb->ptr) = *src;
                sb->ptr++;
                local_remaining--;
        }
    }
    sb->remaining = local_remaining;
}

size_t sb_predict_escaped_length(const char *input) {
    size_t len = 0;
    for(const char *ptr = input; *ptr != 0; ptr++) {
         switch (*ptr) {
            case '&':
                len += 5;
                break;
            case '"':
                len += 6;
                break;
            case '<':
                len += 4;
                break;
            case '>':
                len += 4;
                break;
            default:
                len++;
        }
    }
    return len;
}


void sb_appendf(string_builder_t *sb, const char *format, ...){
    va_list args;
    va_start(args, format);

    int written = vsnprintf(sb->ptr, sb->remaining, format, args);
    if(written >= 0 && written < sb->remaining) {
        sb->ptr += written;
        sb->remaining -= written;
    } else {
        sb->remaining = -1;
    }
    
    va_end(args);
}