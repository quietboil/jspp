#include "jspp.h"
#include <stddef.h>

// Scanner and parser states.
// These are combined with _json_tokens to form a complete set of states.
// Note that some of these are just tags that are defined to make code
// more readable.
enum _states {
    __SCANNER_STATES = 0x20,

    NULL_N,
    NULL_U,
    NULL_L,
    TRUE_T,
    TRUE_R,
    TRUE_U,
    FALSE_F,
    FALSE_A,
    FALSE_L,
    FALSE_S,

    STRING_BEGIN,
    STRING_CHARS,
    STRING_ESC,
    __STRING_END,

    NUMBER_BEGIN,
    INT_DIGITS,
    DEC_DIGITS,
    EXP,
    EXP_DIGITS,
    __NUMBER_END,

    __PARSER_STATES = 0x40,

    EXPECTING_ARRAY_TAIL,
    EXPECTING_OBJECT_TAIL,
    EXPECTING_OBJECT_MEMBER_NAME_VALUE_SEPARATOR,

    __REDUCING_PARSER_STATES = 0x50,
    // The states below will make the parser "reduce" the stack and
    // return a token. Unlike the states above that only "shift" (do
    // not mistake this for a LALR shift) the parser into a new state.

    EXPECTING_JSON,
    EXPECTING_ARRAY_ELEMENT_OR_END,
    EXPECTING_ARRAY_ELEMENT,
    EXPECTING_OBJECT_MEMBER_NAME_OR_END,
    EXPECTING_OBJECT_MEMBER_NAME,
    EXPECTING_OBJECT_MEMBER_VALUE
};

/**
 * \brief "Looks up" the next automaton state.
 *
 * \param state     Current state.
 * \param lookahead Next character in the stream,
 *
 * \return New state
 */
static uint8_t next_scan_state(uint8_t state, uint8_t lookahead)
{
    if (state > __PARSER_STATES) {
        switch (lookahead) {
            case '\t':
            case '\n':
            case '\r':
            case ' ': return state;
        }
    }

    switch (state) {
        case EXPECTING_OBJECT_MEMBER_NAME_OR_END: {
            if (lookahead == '}') return JSON_OBJECT_END;
        }
        case EXPECTING_OBJECT_MEMBER_NAME: {
            if (lookahead == '"') return STRING_BEGIN;
            break;
        }
        case EXPECTING_OBJECT_MEMBER_NAME_VALUE_SEPARATOR: {
            if (lookahead == ':') return EXPECTING_OBJECT_MEMBER_VALUE;
            break;
        }
        case EXPECTING_OBJECT_TAIL: {
            if (lookahead == ',') return EXPECTING_OBJECT_MEMBER_NAME;
            if (lookahead == '}') return JSON_OBJECT_END;
            break;
        }
        case EXPECTING_ARRAY_TAIL: {
            if (lookahead == ',') return EXPECTING_ARRAY_ELEMENT;
            if (lookahead == ']') return JSON_ARRAY_END;
            break;
        }
        case EXPECTING_ARRAY_ELEMENT_OR_END: {
            if (lookahead == ']') return JSON_ARRAY_END;
        }
        case EXPECTING_OBJECT_MEMBER_VALUE:
        case EXPECTING_ARRAY_ELEMENT:
        case EXPECTING_JSON: {
            switch (lookahead) {
                case 'n': return NULL_N;
                case 't': return TRUE_T;
                case 'f': return FALSE_F;
                case '"': return STRING_BEGIN;
                case '{': return JSON_OBJECT_BEGIN;
                case '[': return JSON_ARRAY_BEGIN;
                case '-': return NUMBER_BEGIN;
            }
            if ('0' <= lookahead && lookahead <= '9') {
                return NUMBER_BEGIN;
            }
            break;
        }
        case NULL_N: {
            if (lookahead == 'u') return NULL_U;
            break;
        }
        case NULL_U: {
            if (lookahead == 'l') return NULL_L;
            break;
        }
        case NULL_L: {
            if (lookahead == 'l') return JSON_NULL;
            break;
        }
        case TRUE_T: {
            if (lookahead == 'r') return TRUE_R;
            break;
        }
        case TRUE_R: {
            if (lookahead == 'u') return TRUE_U;
            break;
        }
        case TRUE_U: {
            if (lookahead == 'e') return JSON_TRUE;
            break;
        }
        case FALSE_F: {
            if (lookahead == 'a') return FALSE_A;
            break;
        }
        case FALSE_A: {
            if (lookahead == 'l') return FALSE_L;
            break;
        }
        case FALSE_L: {
            if (lookahead == 's') return FALSE_S;
            break;
        }
        case FALSE_S: {
            if (lookahead == 'e') return JSON_FALSE;
            break;
        }
        case NUMBER_BEGIN:
        case INT_DIGITS: {
            if ('0' <= lookahead && lookahead <= '9') {
                return INT_DIGITS;
            } else if (lookahead == '.') {
                return DEC_DIGITS;
            } else if (lookahead == 'E' || lookahead == 'e') {
                return EXP;
            } else {
                return JSON_INTEGER;
            }
        }
        case DEC_DIGITS: {
            if ('0' <= lookahead && lookahead <= '9') {
                return DEC_DIGITS;
            } else if (lookahead == 'E' || lookahead == 'e') {
                return EXP;
            } else {
                return JSON_DECIMAL;
            }
        }
        case EXP: {
            if ('0' <= lookahead && lookahead <= '9') {
                return EXP_DIGITS;
            } else if (lookahead == '+' || lookahead == '-') {
                return EXP_DIGITS;
            }
            break;
        }
        case EXP_DIGITS: {
            if ('0' <= lookahead && lookahead <= '9') {
                return EXP_DIGITS;
            } else {
                return JSON_FLOATING_POINT;
            }
        }
        case STRING_BEGIN:
        case STRING_CHARS: {
            switch (lookahead) {
                case '\\': return STRING_ESC;
                case '"':  return JSON_STRING;
                default:   return STRING_CHARS;
            }
        }
        case STRING_ESC: {
            return STRING_CHARS;
        }
    }
    return JSON_INVALID;
}

/**
 * \brief Determines the next parser state to enter after it "reduces" the stack (and returns a token)
 *
 * \param state Current parser state
 *
 * \return Next machine state (always a parser state)
 *
 * Normally something like a LALR parser would also need a reduced symbol to determine the next state.
 * In this case we can skip it as we always return a token and (1) what we returned is constrained by
 * the current state and (2) when there is a difference (null vs number vs string, etc.) it is not
 * important as they are all just JSON symbols in the grammar anyway.
 */
static uint8_t next_parsing_state(uint8_t state)
{
    switch (state) {
        case JSON_OBJECT_BEGIN: {
            return EXPECTING_OBJECT_MEMBER_NAME_OR_END;
        }
        case EXPECTING_OBJECT_MEMBER_NAME_OR_END:
        case EXPECTING_OBJECT_MEMBER_NAME: {
            return EXPECTING_OBJECT_MEMBER_NAME_VALUE_SEPARATOR;
        }
        case EXPECTING_OBJECT_MEMBER_VALUE: {
            return EXPECTING_OBJECT_TAIL;
        }
        case JSON_ARRAY_BEGIN: {
            return EXPECTING_ARRAY_ELEMENT_OR_END;
        }
        case EXPECTING_ARRAY_ELEMENT_OR_END:
        case EXPECTING_ARRAY_ELEMENT: {
            return EXPECTING_ARRAY_TAIL;
        }
        case EXPECTING_JSON: {
            return JSON_END;
        }
    }
    return JSON_INVALID;
}

static inline uint8_t get_state(jspp_t * parser)
{
    return parser->stack[parser->level];
}

static inline void set_state(jspp_t * parser, uint8_t state)
{
    parser->stack[parser->level] = state;
}

/**
 * \brief Checks whether the current scanning state represents a token that should be returned.
 *
 * \param state Current machine state.
 *
 * \return "true" when state represents a token.
 */
static inline int is_final(uint8_t state)
{
    return state < __SCANNER_STATES;
}

static void set_token_start(jspp_t * parser, uint8_t state, const char * txt)
{
    parser->token_start = txt - parser->text;
    if (state == STRING_BEGIN) {
        // exclude opening '"' from the token text
        ++parser->token_start;
    }
}

static void set_token_end(jspp_t * parser, uint8_t token, const char * txt)
{
    parser->token = token;
    // adjust the end of token pointer
    switch (token) {
        case JSON_NULL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_OBJECT_BEGIN:
        case JSON_OBJECT_END:
        case JSON_ARRAY_BEGIN:
        case JSON_ARRAY_END: {
            // "move" pointer to the next character (after the last token character)
            ++txt;
        }
    }
    parser->token_length = txt - parser->text - parser->token_start;
}

///< Returns true if a (scan) state represents a beginning of the multi-character token.
///< In terms of the usage - whether parser will need to "shift" scanning.
static inline int is_token_start(uint8_t state)
{
    return state == NULL_N
        || state == TRUE_T
        || state == FALSE_F
        || state == NUMBER_BEGIN
        || state == STRING_BEGIN
        ;
}

///< Returns true if a state represents a token that begins a composite JSON (and that
///< requires a "shift")
static inline int is_nested_level_start(uint8_t state)
{
    return state == JSON_ARRAY_BEGIN
        || state == JSON_OBJECT_BEGIN
        ;
}

///< Object memeber names are scanned as strings, but when parser expects a member name, which is
///< a string :-), we return special tokens. This function tells is if we are in one of those states.
static inline int is_string_a_member_name(uint8_t state)
{
    return state == EXPECTING_OBJECT_MEMBER_NAME
        || state == EXPECTING_OBJECT_MEMBER_NAME_OR_END
        ;
}

uint8_t jspp_next(jspp_t * parser)
{
    if (parser->level >= JSON_MAX_STACK) {
        return JSON_TOO_DEEP;
    }

    uint8_t state = get_state(parser);
    if (state <= JSON_END) {
        return state;
    }

    const char * const end = parser->text + parser->text_length;
    const char * txt = parser->text + parser->token_start + parser->token_length;
    if (parser->token == JSON_STRING) {
        // move past the closing '"'
        ++txt;
    }
    if (txt == end) {
        return JSON_CONTINUE;
    }

    do {
        state = next_scan_state(state, *txt);
        if (is_token_start(state) || is_nested_level_start(state)) {
            set_token_start(parser, state, txt);
            if (++parser->level == JSON_MAX_STACK) {
                return JSON_TOO_DEEP;
            }
        } else if (state > __REDUCING_PARSER_STATES) {
            set_state(parser, state);
        }
    } while (!is_final(state) && ++txt < end);

    if (is_final(state)) {
        uint8_t token = state;
        if (token == JSON_ARRAY_END || token == JSON_OBJECT_END) {
            // These 2 have not had their start offsets set yet. Do that now.
            set_token_start(parser, token, txt);
        }
        set_token_end(parser, token, txt);

        if (!is_nested_level_start(state)) {
            --parser->level;
            state = get_state(parser);
            if (token == JSON_STRING && is_string_a_member_name(state)) {
                token = JSON_MEMBER_NAME;
            }
        }
        state = next_parsing_state(state);
        set_state(parser, state);
        return token;
    }
    // if the (scanner) state is not final, then we are in the middle of a token scanning
    // and need to set the next start state to the current scanner state, so the machine
    // would continue where it left off
    set_state(parser, state);
    set_token_end(parser, state, end);

    if (STRING_BEGIN <= state && state < __STRING_END) {
        uint8_t prev_level_state = parser->stack[parser->level - 1];
        if (is_string_a_member_name(prev_level_state)) {
            return JSON_MEMBER_NAME_PART;
        }
        return JSON_STRING_PART;
    } else if (NUMBER_BEGIN <= state && state < __NUMBER_END) {
        return JSON_NUMBER_PART;
    } else {
        return JSON_CONTINUE;
    }
}

///< This function skips objects and arrays.
static uint8_t skip_composite(jspp_t * parser)
{
    uint8_t token;
    while ((token = jspp_next(parser)) != parser->skip_token || parser->level > parser->skip_level) {
        if (token == JSON_CONTINUE) {
            return JSON_CONTINUE;
        }
    }
    parser->skip_token = 0;
    return jspp_next(parser);
}

uint8_t jspp_skip_next(jspp_t * parser)
{
    uint8_t token = jspp_next(parser);
    if (token <= JSON_END) {
        parser->skip_token = 0;
        return token;
    }
    if (token <= JSON_MEMBER_NAME_PART) {
        parser->skip_level = parser->level;
        parser->skip_token = JSON_CONTINUE;
        return JSON_CONTINUE;
    }
    switch (token) {
        case JSON_MEMBER_NAME: {
            return jspp_skip_next(parser);
        }
        case JSON_ARRAY_BEGIN: {
            parser->skip_level = parser->level - 1;
            parser->skip_token = JSON_ARRAY_END;
            return skip_composite(parser);
        }
        case JSON_OBJECT_BEGIN: {
            parser->skip_level = parser->level - 1;
            parser->skip_token = JSON_OBJECT_END;
            return skip_composite(parser);
        }
        case JSON_ARRAY_END:
        case JSON_OBJECT_END: {
            // these alone should not be skipped
            parser->skip_token = 0;
            return token;
        }
    }
    parser->skip_token = 0;
    return jspp_next(parser);
}

const char * jspp_text(jspp_t * parser, uint16_t * token_length)
{
    *token_length = parser->token_length;
    return parser->text + parser->token_start;
}

uint8_t jspp_start(jspp_t * parser, const char * text, uint16_t text_len)
{
    parser->text = text;
    parser->text_length = text_len;
    parser->token_start = 0;
    parser->token_length = 0;
    parser->token = JSON_INVALID;
    parser->skip_token = 0;
    parser->skip_level = 0;
    parser->level = 0;
    parser->stack[parser->level] = EXPECTING_JSON;

    return jspp_next(parser);
}

uint8_t jspp_continue(jspp_t * parser, const char * text, uint16_t text_len)
{
    parser->text = text;
    parser->text_length = text_len;
    parser->token_start = 0;
    parser->token_length = 0;
    parser->token = JSON_INVALID;

    switch (parser->skip_token) {
        case JSON_CONTINUE: {
            return jspp_skip_next(parser);
        }
        case JSON_ARRAY_END:
        case JSON_OBJECT_END: {
            return skip_composite(parser);
        }
    }
    return jspp_next(parser);
}
