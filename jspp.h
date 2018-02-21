#ifndef __JSPP_H
#define __JSPP_H

#define JSON_MAX_STACK 14

#include <stdint.h>

enum _json_tokens {
    JSON_INVALID,           ///< Something is wrong with the JSON.
    JSON_TOO_DEEP,          ///< JSON has too many levels of nested elements (more than the parser can handle as configured)
    JSON_END,               ///< Returned when parser completely processed a valid JSON (even if there is a trailing text aftet it).
    JSON_CONTINUE,          ///< The current text fragment ended before the entire JSON was parsed.
                            ///< Next text fragment is needed to continue.

    JSON_MEMBER_NAME_PART,  ///< The (first) part of the object member name split at the fragment boundary
    JSON_NUMBER_PART,       ///< Partially recognized number. Returned when a number crosses the text fragment boundaries
                            ///< Specific type (integer, decimal, floating point) will be returned when number is completly
                            ///< recognized. Caller is expected to save this partial result and then combine it with the
                            ///< rest of the number from the next text fragment.
    JSON_STRING_PART,       ///< String that spans multiple (at least 2) text fragments

    JSON_NULL,
    JSON_TRUE,
    JSON_FALSE,

    JSON_INTEGER,
    JSON_DECIMAL,           ///< Has a decimal point, but no exponent. This separate type exists because in some cases it makes
                            ///< sense to treat these as fixed point numbers.
    JSON_FLOATING_POINT,    ///< A floating point number with exponent.
    JSON_STRING,
    JSON_MEMBER_NAME,

    JSON_OBJECT_BEGIN,
    JSON_OBJECT_END,

    JSON_ARRAY_BEGIN,
    JSON_ARRAY_END
};

typedef struct _json_parser {
    const char *  text;         ///< JSON text fragment
    uint16_t      text_length;  ///< Size of the text fragment
    uint16_t      token_start;  ///< Index of the first character of the token text
    uint16_t      token_length; ///< Token text length. Note that quotes are not a part of the string/member name token text.
    uint8_t       token;
    uint8_t       skip_token;   ///< Hint left by `skip` for `continue` to continue skipping
    uint8_t       skip_level;   ///< Skip termination level
    uint8_t       level;        ///< Current stack level
    uint8_t       stack[JSON_MAX_STACK];
} jspp_t;

/**
 * \brief Initializes the parser and returns the first token.
 *
 * \param parser   A pointer to the parser struct allocated by the caller
 * \param text     The initial JSON text fragment
 * \param text_len The length of the initial text
 *
 * \return The token ID
 *
 * This function initializes the JSON parser. The parser (state) struct has to be allocated
 * by the caller. For example:
 *
 *     static jspp_t parser;
 *     void network_packet_received_event_handler(const char * data, size_t data_length)
 *     {
 *         uint8_t token = jspp_start(&parser, data, data_length);
 *         // ...
 */
uint8_t jspp_start(jspp_t * parser, const char * text, uint16_t text_len);

/**
 * \brief Feeds next JSON fragment to the parser
 *
 * \param parser    A pointer to the parser struct that was previously initialized by the `jspp_start`
 * \param text      The next JSON text fragment
 * \param text_len  The length of the text
 *
 * \return The token ID
 *
 * This function is called to continue parsing JSON when the next text fragment becomes available.
 */
uint8_t jspp_continue(jspp_t * parser, const char * text, uint16_t text_len);

/**
 * \brief Returns the ID of the next token found in the current JSON text fragment
 *
 * \param parser  A pointer to the parser struct
 *
 * \return The token ID
 */
uint8_t jspp_next(jspp_t * parser);

/**
 * \brief Returns a pointer to the text of the current token
 *
 * \param      parser  A pointer to the parser struct
 * \param[out] length  A pointer to the variable in which the length of the token text will be returned
 *
 * \return Pointer to the token text.
 */
const char * jspp_text(jspp_t * parser, uint16_t * length);

/**
 * \brief Skips the next JSON element and returns the token that follows the skipped element.
 *
 * \param parser A pointer to the parser struct
 *
 * \return The ID of the token after the skipped element
 *
 * This function skips the next element (not token!).
 *
 * Note that the skipped element can be a literal (null, false, true), a number, a string, an array,
 * an object (objects and arrays are still affected by the `JSON_MAX_STACK` that limits the overall
 * nesting) or an object member (name-value pair).
 *
 * Note also that while an element is being skipped this function might reach the end of the current text
 * fragment. In this case, like `jspp_next`, it'll return `JSON_CONTINUE` and the caller, just like
 * with the same result from `jspp_next`, is expected to call `jspp_continue` when the next
 * fragment becomes available. Provided that the element that is being skipped is big enought this process -
 * handling `JSON_CONTINUE` - might be repeated multiple times even before eventually `jspp_continue`
 * would return a token after the skipped element.
 */
uint8_t jspp_skip_next(jspp_t * parser);

/**
 * \brief Skips the current JSON element and returns the token that follows the skipped element.
 *
 * \param parser A pointer to the parser struct
 *
 * \return The ID of the token after the skipped element
 *
 * This function skips the current element. It is mostly useful to skip the detected but unexpected
 * objects or arrays. It can also skip the rest of the partially returned numbers and strings. In cases,
 * when the current element has been completely scanned, there will be nothing to skip, so the function
 * just returns the next token.
 */
uint8_t jspp_skip(jspp_t * parser);

#endif