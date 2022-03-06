#pragma once

typedef struct {
    char *key;
    char *value;
} form_parameter_t;

/**
 * @brief Split a FORM Encoded input into parameters.
 * 
 * @param input input string null terminated. Will be modified to insert nulls
 * @param result buffer for result pointers
 * @param max_parameters number of entries in the result buffer
 * @return int the number of parameters found. Will be max_parameters+1 if more were found.
 */
int read_form_parameters(char *input, form_parameter_t *result, int max_parameters);

