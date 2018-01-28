#include <httpget.h>
#include <jspp.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

typedef struct _time {
    uint16_t  sec:  6;
    uint16_t  min:  6;
    uint16_t  hour: 4;  // 12-hour format. AM or PM is determined from the context
} ws_time_t;

/// What we need (to extract) from the WS response
typedef struct _ws_data {
    ws_time_t   twilight_begin;
    ws_time_t   sunrise;
    ws_time_t   sunset;
    ws_time_t   twilight_end;
} sunset_sunrize_data_t;

/// A callback that will be called when the data application needs have been extracted
typedef void (*sunset_sunrize_data_handler_t)(sunset_sunrize_data_t data);

/// 
typedef struct _ws_resp {
    jspp_t    json_parser;
    uint8_t   state;        // Need to known where we are, so we can continue from that point if we are interrupted
    uint8_t   text_length;  // Length of the split token text
    char      text[30];     // Buffer to collect parts of the split token.
                            // Note that the longest possible test in the response is 27 characters long.
    //
    // The "overflow" text buffer is used to simplify processing when parser returns a partial token -
    // a member name or a string. Instead of trying to process it in parts we will copy both of them
    // into this buffer and then deal with the result as if it was a whole token. I.o.w. this would
    // allow the response handler to (almost) pretend that the response fragments are magically split
    // at insignificant whitespaces.
    //
    // This approach cannot be applied in every case. For instance, when the strings are very large.
    // However it can be used at least for member names which usually are reasonably short. Also in
    // cases that are similar to this example, where the longest "value" string is also short, it can
    // be used for both - member names and member values.
    //
    // Alternatively we could avoid this text buffer (and probably make parsing somewhat more efficient
    // at the same time) if we build an automaton (or maybe several specialized automata) to recognize
    // JSON elements. However that would make this example quite a bit more verbose and likely more
    // difficult to understand. Therefore that implementation is left as an excerise for the reader ;-)
    //
    sunset_sunrize_data_t           ws_data;
    sunset_sunrize_data_handler_t   ws_data_handler;
} sunset_sunrize_resp_t;

enum _ws_resp_states {
    SKIPPING_RESPONSE_HEADER = 0, // this way it is "set" automaticaly by calloc
    RESPONSE_HEADER_TERM_CR_FOUND,
    RESPONSE_HEADER_TERM_LF_FOUND,
    HEADERS_TERM_CR_FOUND,
    HEADERS_SKIPPED,
    EXPECTING_RESULTS,
    EXPECTING_RESULTS_OBJECT,
    EXPECTING_DATA_NAME,
    EXPECTING_DATA_VALUE,
    EXPECTING_SUNRISE_VALUE,
    EXPECTING_SUNSET_VALUE,
    EXPECTING_TWILIGHT_BEGIN_VALUE,
    EXPECTING_TWILIGHT_END_VALUE,
    DONE,
    PARSING_FAILED
};

/**
 * \brief Scans the string until the first non-dogot character or the end marker.
 * 
 * \param ptr Pointer to the beginning of the string
 * \param end The end of the string pointer
 * \paran val Pointer to the variables where the scanned results will be stored
 * 
 * \return Updated pointer. It'll point to the first non-digit character that terminated this scan
 *         or to the end of the string.
 */
static const char * scan_int(const char * ptr, const char * const end, uint16_t * val)
{
    uint16_t v = 0;
    while (ptr < end && '0' <= *ptr && *ptr <= '9') {
        v = v * 10 + (*ptr++ - '0');
    }
    *val = v;
    return ptr;
}

/**
 * \brief Scans the time string.
 * 
 * \param str Pointer to the beginning of the string
 * \param len The string length
 * 
 * \return Scanned time value.
 * 
 * The expected tring format is %d:%d:%d (or %H:%M:%S). THe scan will terminate early if
 * the input string is not formatted properly. Fields that were not scanned will be set to 0.
 */
static ws_time_t scan_time(const char * str, uint16_t len)
{
    uint16_t h = 0, m = 0, s = 0;
    const char * end = str + len;
    str = scan_int(str, end, &h);
    if (*str == ':') {
        str = scan_int(str + 1, end, &m);
        if (*str == ':') {
            scan_int(str + 1, end, &s);
        }
    }
    return (ws_time_t){s, m, h};
}

/**
 * \brief Skips response headers.
 *
 * \param data    Pointer to the first byte of the data fragment
 * \param end     The end-of-fragment pointer
 * \param ws_resp Pointer to the WS response handling state machine
 *
 * \return The pointer to the first payload (JSON) byte. Or, if
 *         the end of headers is not encountered in the current
 *         fragment, returns the end-of-fragment pointer.
 */
static const char * skip_response_headers(const char * data, const char * end, sunset_sunrize_resp_t * ws_resp)
{
    while (data < end && ws_resp->state < HEADERS_SKIPPED) {
        switch (ws_resp->state) {
            case SKIPPING_RESPONSE_HEADER: {
                if (*data == '\r') {
                    ws_resp->state = RESPONSE_HEADER_TERM_CR_FOUND;
                }
                break;
            }
            case RESPONSE_HEADER_TERM_CR_FOUND: {
                if (*data == '\n') {
                    ws_resp->state = RESPONSE_HEADER_TERM_LF_FOUND;
                }
                break;
            }
            case RESPONSE_HEADER_TERM_LF_FOUND: {
                if (*data == '\r') {
                    ws_resp->state = HEADERS_TERM_CR_FOUND;
                } else {
                    ws_resp->state = SKIPPING_RESPONSE_HEADER;
                }
                break;
            }
            case HEADERS_TERM_CR_FOUND: {
                if (*data == '\n') {
                    ws_resp->state = HEADERS_SKIPPED;
                }
                break;
            }
        }
        ++data;
    }
    return data;
}

/**
 * \brief Checks whether the specified tocken matches the expected one.
 *
 * \param      token       The ID of the parser returned token
 * \param      expected    The ID of the expected token
 * \param      next_state  The state to switch to if the parser returned the expected token
 * \param[out] text_len    Pointer to the variable that will hold the length of the token text
 * \param      ws_resp     Pointer to the WS response handling state machine
 *
 * \return pointer to the token text if the expectation holds and handler should continue processing the response
 *         NULL if the token does not match the expected one and the handler should return
 *
 * This function also sets status of the response processing to failed if it detects that the returned token is
 * unexpected. The latter makes the handler skip the remaining fragments if any.
 */
static const char * expected(uint8_t token, uint8_t expected, uint8_t next_state, uint16_t * text_len, sunset_sunrize_resp_t * ws_resp)
{
    uint16_t len;
    const char * text;

    if (token == expected) {
        ws_resp->state = next_state;
        if (ws_resp->text_length == 0) {
            return jspp_text(&ws_resp->json_parser, text_len);
        }
        text = jspp_text(&ws_resp->json_parser, &len);
        if (len + ws_resp->text_length > sizeof(ws_resp->text)) {
            ws_resp->state = PARSING_FAILED;
            ws_resp->text_length = 0;
            return NULL;
        }
        memcpy(ws_resp->text + ws_resp->text_length, text, len);
        *text_len = ws_resp->text_length + len;
        ws_resp->text_length = 0; // prepare for the next token
        return ws_resp->text;
    }

    if (token == JSON_NUMBER_PART && JSON_INTEGER <= expected && expected <= JSON_FLOATING_POINT
        ||
        token == JSON_STRING_PART && expected == JSON_STRING
        ||
        token == JSON_MEMBER_NAME_PART && expected == JSON_MEMBER_NAME
    ) {
        uint16_t len;
        const char * text = jspp_text(&ws_resp->json_parser, &len);
        if (len > sizeof(ws_resp->text)) {
            ws_resp->state = PARSING_FAILED;
        } else {
            ws_resp->text_length = len;
            memcpy(ws_resp->text, text, len);
        }
        return NULL;
    }

    if (token != JSON_CONTINUE) {
        ws_resp->state = PARSING_FAILED;
    }
    return NULL;
}

/**
 * \brief Handles incoming response fragments.
 *
 * \param data    Pointer to the first byte of the data fragment
 * \param size    The size of the data in the current fragment
 * \param ws_resp Pointer to the WS call processing state machine
 *
 * The handler initially skips response headers. Because "http_get" sends 1.0 requests
 * (to avoid dealing with chunked data as that "library" designed to only serve this
 * example :) there is nothing interesting or useful we can find in those headers.
 *
 * Once JSON payload is encountered this handler would start the parser and extract the
 * data it is interesed in - the today's twilight times.
 */
static void handle_ws_response(const char * data, uint16_t size, sunset_sunrize_resp_t * ws_resp)
{
    const char * end = data + size;
    if (ws_resp->state < HEADERS_SKIPPED) {
        data = skip_response_headers(data, end, ws_resp);
        size = end - data;
    }

    if (ws_resp->state >= DONE) {
        // Even though we are DONE with the data, we stil might receive more payload fragments
        return;
    }

    jspp_t * parser = &ws_resp->json_parser;
    uint8_t token;

    if (ws_resp->state == HEADERS_SKIPPED) {
        token = jspp_start(parser, data, size);
    } else {
        token = jspp_continue(parser, data, size);
    }

    switch (ws_resp->state) {
        case EXPECTING_RESULTS:                 goto EXPECTING_RESULTS;
        case EXPECTING_RESULTS_OBJECT:          goto EXPECTING_RESULTS_OBJECT;
        case EXPECTING_DATA_NAME:               goto EXPECTING_DATA_NAME;
        case EXPECTING_DATA_VALUE:              goto EXPECTING_DATA_VALUE;
        case EXPECTING_SUNRISE_VALUE:           goto EXPECTING_SUNRISE_VALUE;
        case EXPECTING_SUNSET_VALUE:            goto EXPECTING_SUNSET_VALUE;
        case EXPECTING_TWILIGHT_BEGIN_VALUE:    goto EXPECTING_TWILIGHT_BEGIN_VALUE;
        case EXPECTING_TWILIGHT_END_VALUE:      goto EXPECTING_TWILIGHT_END_VALUE;
    }

    const char * text;
    uint16_t text_length;

    if (!expected(token, JSON_OBJECT_BEGIN, EXPECTING_RESULTS, &text_length, ws_resp)) return;
    token = jspp_next(parser);

EXPECTING_RESULTS:
    text = expected(token, JSON_MEMBER_NAME, EXPECTING_RESULTS_OBJECT, &text_length, ws_resp);
    if (!text) return;
    if (strncmp("results", text, text_length) != 0) {
        ws_resp->state = PARSING_FAILED;
        return;
    }
    token = jspp_next(parser);

EXPECTING_RESULTS_OBJECT:
    if (!expected(token, JSON_OBJECT_BEGIN, EXPECTING_DATA_NAME, &text_length, ws_resp)) return;
    token = jspp_next(parser);

EXPECTING_DATA_NAME:
    // Note that we are in a "loop" here, so check the termination condition (the end of object) first
    if (token == JSON_OBJECT_END) {
        // note that this jsut the end of the "results", but we are not interested in anything else, thus...
        ws_resp->state = DONE;
        goto DONE;
    }
    text = expected(token, JSON_MEMBER_NAME, EXPECTING_DATA_VALUE, &text_length, ws_resp);
    if (!text) return;

    // In this examples all the values that we are interested in have the same type. This allows us
    // to use a simpler implementation than what is shown below. For example, remember which member
    // we are dealing with and parse its value in EXPECTING_DATA_VALUE state. However in general,
    // when the element types are different, we need to be prepared to deal with different cases.
    // That's why this example uses different states for different members.
    if (strncmp("sunrise", text, text_length) == 0) {
        ws_resp->state = EXPECTING_SUNRISE_VALUE;
    } else if (strncmp("sunset", text, text_length) == 0) {
        ws_resp->state = EXPECTING_SUNSET_VALUE;
    } else if (strncmp("civil_twilight_begin", text, text_length) == 0) {
        ws_resp->state = EXPECTING_TWILIGHT_BEGIN_VALUE;
    } else if (strncmp("civil_twilight_end", text, text_length) == 0) {
        ws_resp->state = EXPECTING_TWILIGHT_END_VALUE;
    }

    token = jspp_next(parser);
    switch (ws_resp->state) {
        case EXPECTING_SUNRISE_VALUE:           goto EXPECTING_SUNRISE_VALUE;
        case EXPECTING_SUNSET_VALUE:            goto EXPECTING_SUNSET_VALUE;
        case EXPECTING_TWILIGHT_BEGIN_VALUE:    goto EXPECTING_TWILIGHT_BEGIN_VALUE;
        case EXPECTING_TWILIGHT_END_VALUE:      goto EXPECTING_TWILIGHT_END_VALUE;
    }

EXPECTING_DATA_VALUE:
    // While we do not need the member value in this state (as we have dedicated states for values we are
    // interested in) we still have to deal with cases when that unneeded value is split between two fragments
    // In general we woudl need a version of `expected` to tell us if we can loop back to expecting next
    // member name or shoudl return and wait for the next fragment. In this example though all values are of the same
    // type - strings - so we can piggy-back on `expected` to tell us how to handle it.
    if (!expected(token, JSON_STRING, EXPECTING_DATA_NAME, &text_length, ws_resp)) return;
    token = jspp_next(parser);
    goto EXPECTING_DATA_NAME;   // Recall that this is a state machine, so we loop with the goto

EXPECTING_SUNRISE_VALUE:
    text = expected(token, JSON_STRING, EXPECTING_DATA_NAME, &text_length, ws_resp);
    if (!text) return;
    ws_resp->ws_data.sunrise = scan_time(text, text_length);

    token = jspp_next(parser);
    goto EXPECTING_DATA_NAME;

EXPECTING_SUNSET_VALUE:
    text = expected(token, JSON_STRING, EXPECTING_DATA_NAME, &text_length, ws_resp);
    if (!text) return;
    ws_resp->ws_data.sunset = scan_time(text, text_length);

    token = jspp_next(parser);
    goto EXPECTING_DATA_NAME;

EXPECTING_TWILIGHT_BEGIN_VALUE:
    text = expected(token, JSON_STRING, EXPECTING_DATA_NAME, &text_length, ws_resp);
    if (!text) return;
    ws_resp->ws_data.twilight_begin = scan_time(text, text_length);

    token = jspp_next(parser);
    goto EXPECTING_DATA_NAME;

EXPECTING_TWILIGHT_END_VALUE:
    text = expected(token, JSON_STRING, EXPECTING_DATA_NAME, &text_length, ws_resp);
    if (!text) return;
    ws_resp->ws_data.twilight_end = scan_time(text, text_length);

    token = jspp_next(parser);
    goto EXPECTING_DATA_NAME;

DONE:
    ws_resp->ws_data_handler(ws_resp->ws_data);
    return;
}


// For this example we allocate the WS call procesing structure statically. In fact it could be the
// way to go even in the real application if, for example, the device is only capable of handling
// one request-response at a time. Or if the use cases that the application supports do not need
// to handle more than one WS call at a time.
// Even if the device can and need to issue and process more than one WS call simultaneously we can
// allocate a pool of these (maybe parsers and WS calls separately then) and allocate from that pool.
// We could also allocate this structure dynamically (malloc). Note that to safely dispose/return
// those we'll need a disconnect callback from the OS/library.
static sunset_sunrize_resp_t ws_resp;

static void process_sunset_sunrize_data(sunset_sunrize_data_t data)
{
    printf("twilight begin: %2d:%02d:%02d\n", data.twilight_begin.hour, data.twilight_begin.min, data.twilight_begin.sec);
    printf("       sunrise: %2d:%02d:%02d\n", data.sunrise.hour, data.sunrise.min, data.sunrise.sec);
    printf("        sunset: %2d:%02d:%02d\n", data.sunset.hour, data.sunset.min, data.sunset.sec);
    printf("  twilight end: %2d:%02d:%02d\n", data.twilight_end.hour, data.twilight_end.min, data.twilight_end.sec);
}

int main(int argc, char * argv[])
{
    const char * request = (argc == 2 ? argv[1] : "/json?lat=38.889411&lng=-77.0352381");

    // Initialize WS response structure.
    // This is irrelevant for this example as the response struct is in .bss and as it is used only once.
    // However in general case we need to do this to reset/clean it in preprartion to process the next response.
    ws_resp.ws_data.twilight_begin = (ws_time_t){0, 0, 0};
    ws_resp.ws_data.sunrise = (ws_time_t){0, 0, 0};
    ws_resp.ws_data.sunset = (ws_time_t){0, 0, 0};
    ws_resp.ws_data.twilight_end = (ws_time_t) {0, 0, 0};
    ws_resp.ws_data_handler = process_sunset_sunrize_data;
    ws_resp.state = SKIPPING_RESPONSE_HEADER;
    ws_resp.text_length = 0;

    if (http_get("api.sunrise-sunset.org", request, (http_get_cb_t)handle_ws_response, &ws_resp) != 0) {
        printf("HTTP GET failed\n");
    }
    return 0;
}
