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
int split_parameters(char *input, form_parameter_t *result, int max_parameters);


/**
 * @brief URL Decode a string by interpreting %ab values within it
 * 
 * @param output buffer for result
 * @param input input string
 * @param len  length of the buffer
 * @return int number of characters output or -1 for overflow or format error
 */
int decode_url_encoded(char *output, const char *input, size_t len);
 
size_t predict_decoded_length(const char *input);