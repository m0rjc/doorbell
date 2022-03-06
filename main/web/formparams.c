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
int read_form_parameters(char *input, form_parameter_t *result, int max_parameters) {
    result[0].key = input;
    result[0].value = NULL;
    int current_index = 0;

    char hex[3];

    char *readptr, *writeptr;
    for(readptr = writeptr = input; *readptr != 0 && current_index < max_parameters; readptr++, writeptr++) {
        switch (*readptr) {
            case '=':
                *writeptr = 0;
                result[current_index].value = writeptr+1;
                break;
            case '&':
                *writeptr = 0;
                current_index++;
                if(current_index < max_parameters) {
                    result[current_index].key = writeptr+1;
                    result[current_index].value = NULL;
                }
                break;
            case '%':
                hex[0] = *(++readptr);
                hex[1] = *(++readptr);
                hex[2] = 0;
                if(!isxdigit(hex[0]) || !isxdigit(hex[1])) return -1;
                *writeptr = 16*READHEX(hex[0]) + READHEX(hex[1]);
                break;
            case '+':
                *writeptr = ' ';
                break;
            default:   
                *writeptr = *readptr;
        }
    }
    // Ensure the result is null terminated.
    *writeptr=0;

    // The state engine can be broken if someone sent two & without an =
    // This will give an uninitialised pointer.
    // If they sent two = we just lose part of the input but that's harmless
    for(int i = 0; i <= current_index && i < max_parameters; i++) {
        if(result[i].value == NULL) {
            // Set an empty string by finding a convenient null terminator to point to.
            for(result[i].value = result[i].key; *(result[i].value) != 0; result[i].value++);
        }
    }

    return current_index + 1;
}

