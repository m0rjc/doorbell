#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "formparams.h"

/** If we know it's [0-9A-Fa-f] convert it to a hex value */
#define READHEX(c) ( ((c)&0x0F) + (((c)&0xF0)==0x30 ? 0 : 9 ) )

/**
 * @brief Split a FORM Encoded input into parameters.
 * 
 * @param input input string null terminated. Will be modified to insert nulls
 * @param result buffer for result pointers
 * @param max_parameters number of entries in the result buffer
 * @return int the number of parameters found. Will be max_parameters+1 if more were found.
 */
int split_parameters(char *input, form_parameter_t *result, int max_parameters) {
    memset(result, 0, max_parameters * sizeof(form_parameter_t));
    result[0].key = input;
    int current_index = 0;

    for(char *ptr = input; *ptr != 0; ptr++) {
        if(*ptr == '=') {
            *ptr = 0;
            ptr++;
            result[current_index].value = ptr;
        } else if(*ptr == '&') {
            *ptr = 0;
            ptr++;
            current_index++;
            if(current_index >= max_parameters) break;
            result[current_index].key = ptr;
        }
    }

    if(current_index < max_parameters && result[current_index].value == NULL) {
        result[current_index].key = NULL;
        current_index--;
    }

    return current_index + 1;
}

/**
 * @brief URL Decode a string by interpreting %ab values within it
 * 
 * @param output buffer for result
 * @param input input string
 * @param len  length of the buffer
 * @return int number of characters output or -1 for overflow or format error
 */
int decode_url_encoded(char *output, const char *input, size_t len) {
    int destIndex = 0;
    char hex[3];
    for(const char *ptr = input; ptr != 0 && destIndex < len; ptr++) {
        switch (*ptr) {
            case'%':
                hex[0] = *(++ptr);
                hex[1] = *(++ptr);
                hex[2] = 0;
                if(!isxdigit(hex[0]) || !isxdigit(hex[1])) return -1;
                output[destIndex++] = 16*READHEX(hex[0]) + READHEX(hex[1]);
                break;
            case '+':
                output[destIndex++] = ' ';
                break;
            default:   
                output[destIndex++] = *ptr;
        }
    }
    if(destIndex < len) {
        output[destIndex++] = 0;
        return destIndex;
    }
    return -1;
}

size_t predict_decoded_length(const char *input) {
    size_t len = 0;
    for(const char *ptr = input; *ptr != 0; ptr++) {
        len++;
        if(*ptr == '%') {
            if( *(++ptr) == 0 ) break; // Premature end of string
            if( *(++ptr) == 0 ) break;
        }
    }
    return len;
}
