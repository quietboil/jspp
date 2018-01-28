#include "test.h"
#include "jspp.h"
#include <string.h>
#include <stdio.h>

#define check_text(v) \
    text = jspp_text(&parser, &length); \
    check(length == sizeof(v) - 1); \
    check(strncmp(text, v, length) == 0)

static int parse_simple_json()
{
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_NULL == jspp_start(&parser, "null", 4));
    check(JSON_NULL == jspp_start(&parser, "\n    null\n", 10));
    check(JSON_TRUE == jspp_start(&parser, "true", 4));
    check(JSON_FALSE == jspp_start(&parser, "false", 5));
    check(JSON_STRING == jspp_start(&parser, "\n    \"Hello, World!\"\n\n", 22));
    check_text("Hello, World!");
    check(JSON_STRING == jspp_start(&parser, "\n    \"Hello\\n,\\t\\\"World\\\"!\"\n\n", 28));
    check_text("Hello\\n,\\t\\\"World\\\"!");

    return 0;
}

static int parse_split_string()
{
    const char * json[] = {
        "\n    \n    \n    \"\\\"Hello, ",
        "World!\\\" is often used to illustrate",
        "a basic working program.\"\n\n\n"
    };
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_STRING_PART == jspp_start(&parser, json[0], strlen(json[0])));
    check_text("\\\"Hello, ");

    check(JSON_STRING_PART == jspp_continue(&parser, json[1], strlen(json[1])));
    check_text("World!\\\" is often used to illustrate");

    check(JSON_STRING == jspp_continue(&parser, json[2], strlen(json[2])));
    check_text("a basic working program.");

    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_split_null()
{
    const char * json[] = {
        "          nu",
        "ll with some trailing text..."
    };
    jspp_t parser;

    check(JSON_CONTINUE == jspp_start(&parser, json[0], strlen(json[0])));
    check(JSON_NULL == jspp_continue(&parser, json[1], strlen(json[1])));
    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_invalid_elements()
{
    jspp_t parser;
    check(JSON_INVALID == jspp_start(&parser, " NULL  ", 7));
    check(JSON_INVALID == jspp_start(&parser, " nulL  ", 7));
    check(JSON_INVALID == jspp_start(&parser, " True  ", 7));
    check(JSON_INVALID == jspp_start(&parser, " trUe  ", 7));
    check(JSON_INVALID == jspp_start(&parser, " False ", 7));
    check(JSON_INVALID == jspp_start(&parser, " faLse ", 7));
    check(JSON_INVALID == jspp_start(&parser, " falsE ", 7));

    return 0;
}

static int parse_numbers()
{
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_INTEGER == jspp_start(&parser, " 12345 ", 7));
    check_text("12345");
    check(JSON_END == jspp_next(&parser));

    check(JSON_INTEGER == jspp_start(&parser, " -1234 ", 7));
    check_text("-1234");
    check(JSON_END == jspp_next(&parser));

    check(JSON_DECIMAL == jspp_start(&parser, " 12.34 ", 7));
    check_text("12.34");
    check(JSON_END == jspp_next(&parser));

    check(JSON_DECIMAL == jspp_start(&parser, " -1.23 ", 7));
    check_text("-1.23");
    check(JSON_END == jspp_next(&parser));

    check(JSON_FLOATING_POINT == jspp_start(&parser, " 12e34 ", 7));
    check_text("12e34");
    check(JSON_END == jspp_next(&parser));

    check(JSON_FLOATING_POINT == jspp_start(&parser, " 12E34 ", 7));
    check_text("12E34");
    check(JSON_END == jspp_next(&parser));

    check(JSON_FLOATING_POINT == jspp_start(&parser, " 1.2e3 ", 7));
    check_text("1.2e3");
    check(JSON_END == jspp_next(&parser));

    check(JSON_FLOATING_POINT == jspp_start(&parser, " -1.23e-45 ", 11));
    check_text("-1.23e-45");
    check(JSON_END == jspp_next(&parser));

    check(JSON_FLOATING_POINT == jspp_start(&parser, " -1.23e+45 ", 11));
    check_text("-1.23e+45");
    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_split_numbers()
{
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_NUMBER_PART == jspp_start(&parser, " 123456", 7));
    check_text("123456");
    check(JSON_INTEGER == jspp_continue(&parser, "7890   ", 7));
    check_text("7890");
    check(JSON_END == jspp_next(&parser));

    check(JSON_NUMBER_PART == jspp_start(&parser, " 123456", 7));
    check_text("123456");
    check(JSON_DECIMAL == jspp_continue(&parser, "789.0  ", 7));
    check_text("789.0");
    check(JSON_END == jspp_next(&parser));

    check(JSON_NUMBER_PART == jspp_start(&parser, " 1.2345", 7));
    check_text("1.2345");
    check(JSON_FLOATING_POINT == jspp_continue(&parser, "6e-78  ", 7));
    check_text("6e-78");
    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_array()
{
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_ARRAY_BEGIN == jspp_start(&parser, " [ ] ", 5));
    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    check(JSON_ARRAY_BEGIN == jspp_start(&parser, "[[],[]]", 7));
    check(JSON_ARRAY_BEGIN == jspp_next(&parser));
    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_ARRAY_BEGIN == jspp_next(&parser));
    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    check(JSON_ARRAY_BEGIN == jspp_start(&parser, " [ 43, true, \"ok\" ] ", 20));
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("43");
    check(JSON_TRUE == jspp_next(&parser));
    check(JSON_STRING == jspp_next(&parser));
    check_text("ok");
    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    check(JSON_ARRAY_BEGIN == jspp_start(&parser, " [ 29, [ \"yes\", \"no\" ], [ 1, 2.3 ] ] ", 37));

    check(JSON_INTEGER == jspp_next(&parser));
    check_text("29");

    check(JSON_ARRAY_BEGIN == jspp_next(&parser));
    check(JSON_STRING == jspp_next(&parser));
    check_text("yes");
    check(JSON_STRING == jspp_next(&parser));
    check_text("no");
    check(JSON_ARRAY_END == jspp_next(&parser));

    check(JSON_ARRAY_BEGIN == jspp_next(&parser));
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("1");
    check(JSON_DECIMAL == jspp_next(&parser));
    check_text("2.3");
    check(JSON_ARRAY_END == jspp_next(&parser));

    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_split_array()
{
    const char * json[] = {
        " [ 29, [ \"yes\", \"n",
        "o\", \"whatever\" ], [ 1, 2.3 ] ] "
    };
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_ARRAY_BEGIN == jspp_start(&parser, json[0], strlen(json[0])));

    check(JSON_INTEGER == jspp_next(&parser));
    check_text("29");

    check(JSON_ARRAY_BEGIN == jspp_next(&parser));
    check(JSON_STRING == jspp_next(&parser));
    check_text("yes");
    check(JSON_STRING_PART == jspp_next(&parser));
    check_text("n");

    check(JSON_STRING == jspp_continue(&parser, json[1], strlen(json[1])));
    check_text("o");

    check(JSON_STRING == jspp_next(&parser));
    check_text("whatever");

    check(JSON_ARRAY_END == jspp_next(&parser));

    check(JSON_ARRAY_BEGIN == jspp_next(&parser));
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("1");
    check(JSON_DECIMAL == jspp_next(&parser));
    check_text("2.3");
    check(JSON_ARRAY_END == jspp_next(&parser));

    check(JSON_ARRAY_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_object()
{
    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_OBJECT_BEGIN == jspp_start(&parser, " { } ", 5));
    check(JSON_OBJECT_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    check(JSON_OBJECT_BEGIN == jspp_start(&parser, " { \"answer\": 42 } ", 18));
    check(JSON_MEMBER_NAME == jspp_next(&parser));
    check_text("answer");
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("42");
    check(JSON_OBJECT_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    const char json[] =
    "{ \"property\": \"The White House\", "
    "  \"owner\": \"National Park Service\", "
    "  \"address\": { "
    "    \"street\": { "
    "      \"number\": 1600, "
    "      \"name\": \"Pennsylvania Avenue\", "
    "      \"direction\": \"NW\" "
    "    }, "
    "    \"city\": \"Washington\", "
    "    \"region\": \"DC\", "
    "    \"zip\": \"20500\" "
    "  }"
    "}"
    ;
    check(JSON_OBJECT_BEGIN == jspp_start(&parser, json, sizeof(json) - 1));
    check(JSON_MEMBER_NAME == jspp_next(&parser));
    check_text("property");
    check(JSON_STRING == jspp_next(&parser));
    check_text("The White House");
    check(JSON_MEMBER_NAME == jspp_next(&parser));
    check_text("owner");
    check(JSON_STRING == jspp_next(&parser));
    check_text("National Park Service");
    check(JSON_MEMBER_NAME == jspp_next(&parser));
    check_text("address");
        check(JSON_OBJECT_BEGIN == jspp_next(&parser));
        check(JSON_MEMBER_NAME == jspp_next(&parser));
        check_text("street");
            check(JSON_OBJECT_BEGIN == jspp_next(&parser));
            check(JSON_MEMBER_NAME == jspp_next(&parser));
            check_text("number");
            check(JSON_INTEGER == jspp_next(&parser));
            check_text("1600");
            check(JSON_MEMBER_NAME == jspp_next(&parser));
            check_text("name");
            check(JSON_STRING == jspp_next(&parser));
            check_text("Pennsylvania Avenue");
            check(JSON_MEMBER_NAME == jspp_next(&parser));
            check_text("direction");
            check(JSON_STRING == jspp_next(&parser));
            check_text("NW");
            check(JSON_OBJECT_END == jspp_next(&parser));
        check(JSON_MEMBER_NAME == jspp_next(&parser));
        check_text("city");
        check(JSON_STRING == jspp_next(&parser));
        check_text("Washington");
        check(JSON_MEMBER_NAME == jspp_next(&parser));
        check_text("region");
        check(JSON_STRING == jspp_next(&parser));
        check_text("DC");
        check(JSON_MEMBER_NAME == jspp_next(&parser));
        check_text("zip");
        check(JSON_STRING == jspp_next(&parser));
        check_text("20500");
        check(JSON_OBJECT_END == jspp_next(&parser));
    check(JSON_OBJECT_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int parse_split_object()
{
    const char * json[] = {
        " { \"question\": \"What do you get wh",
        "en you multiply six by nine\", \"ans",
        "wer\": 42 } "
    };
#define MAX_MEMBER_NAME_LENGTH 8
    char name[MAX_MEMBER_NAME_LENGTH + 1];
    uint16_t name_length;

    jspp_t parser;
    const char * text;
    uint16_t length;

    check(JSON_OBJECT_BEGIN == jspp_start(&parser, json[0], strlen(json[0])));

    check(JSON_MEMBER_NAME == jspp_next(&parser));
    check_text("question");

    check(JSON_STRING_PART == jspp_next(&parser));
    check_text("What do you get wh");

    // The next call is unnecessary as previous code - PART - already signalled the
    // end of the fragment. However in case it is easier to check for one condition
    // parser also will keep returning CONTINUE until it is fed more data
    check(JSON_CONTINUE == jspp_next(&parser));

    check(JSON_STRING == jspp_continue(&parser, json[1], strlen(json[1])));
    check_text("en you multiply six by nine");

    check(JSON_MEMBER_NAME_PART == jspp_next(&parser));
    text = jspp_text(&parser, &length);
    // assemble it from parts
    strncpy(name, text, length);
    name_length = length;

    check(JSON_CONTINUE == jspp_next(&parser));

    check(JSON_MEMBER_NAME == jspp_continue(&parser, json[2], strlen(json[2])));
    text = jspp_text(&parser, &length);
    // add the tail
    strncpy(name + name_length, text, length);
    name_length += length;
    check(name_length == 6);
    check(strncmp(name, "answer", name_length) == 0);

    check(JSON_INTEGER == jspp_next(&parser));
    check_text("42");

    check(JSON_OBJECT_END == jspp_next(&parser));

    check(JSON_END == jspp_next(&parser));

    return 0;
}

static int skip_elements()
{
    jspp_t parser;
    const char * text;
    uint16_t length;

    const char json1[] = "{ \"status\": \"ok\", \"a\": 1, \"b\": 2, \"c\": 3, \"x\": 42, \"y\": 87, \"z\": 99 }";

    check(JSON_OBJECT_BEGIN == jspp_start(&parser, json1, sizeof(json1) - 1));
    // we know that it starts with the "status", which we do no need, so let's skip it altogether
    check(JSON_MEMBER_NAME == jspp_skip_next(&parser));
    // we are we now?
    check_text("a");
    // skip the value of "a" as well
    uint8_t token = jspp_skip_next(&parser);
    check(JSON_MEMBER_NAME == token);
    // now, let's look for what we need
    while (token == JSON_MEMBER_NAME) {
        text = jspp_text(&parser, &length);
        if (strncmp("x", text, length) == 0) break;
        token = jspp_skip_next(&parser);
    }
    check(strncmp("x", text, length) == 0);
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("42");
    // now look for "z"
    token = jspp_next(&parser);
    while (token == JSON_MEMBER_NAME) {
        text = jspp_text(&parser, &length);
        if (strncmp("z", text, length) == 0) break;
        token = jspp_skip_next(&parser);
    }
    check(strncmp("z", text, length) == 0);
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("99");

    check(JSON_OBJECT_END == jspp_next(&parser));
    check(JSON_END == jspp_next(&parser));

    const char json2[] = "{ \"response\": { \"a\": 1, \"b\": { \"q\": \"aaa\", \"r\": 98.7 }, \"c\": [11,22,33,44],"
        " \"x\": 42 }, \"status\": \"ok\", \"rc\": 97 }";
    check(JSON_OBJECT_BEGIN == jspp_start(&parser, json2, sizeof(json2) - 1));
    // skip "response"
    check(JSON_MEMBER_NAME == jspp_skip_next(&parser));
    check_text("status");
    check(JSON_STRING == jspp_next(&parser));
    check_text("ok");
    check(JSON_OBJECT_END == jspp_skip_next(&parser));
    check(JSON_END == jspp_next(&parser));

    const char json31[] = "{ \"response\": { \"a\": 1, \"b\": { \"q\": \"aaa\", \"r\": 98.7 }, \"c\": [11,2";
    const char json32[] = "2,33,44], \"x\": 42 }, \"status\": \"ok\", \"rc\": 97 }";
    check(JSON_OBJECT_BEGIN == jspp_start(&parser, json31, sizeof(json31) - 1));
    // skip "response"
    check(JSON_CONTINUE == jspp_skip_next(&parser));
    check(JSON_MEMBER_NAME == jspp_continue(&parser, json32, sizeof(json32) - 1));
    check_text("status");
    check(JSON_STRING == jspp_next(&parser));
    check_text("ok");
    check(JSON_OBJECT_END == jspp_skip_next(&parser));
    check(JSON_END == jspp_next(&parser));

    const char json41[] = "{ \"response\": { \"a\": 1, \"b\": { \"q\": \"aaa\", \"r\": 98.7 }, \"c\": [11,22,33,44], \"x\": 42 }, \"sta";
    const char json42[] = "tus\": \"ok\", \"rc\": 97 }";
    check(JSON_OBJECT_BEGIN == jspp_start(&parser, json41, sizeof(json41) - 1));
    // skip "response"
    check(JSON_MEMBER_NAME_PART == jspp_skip_next(&parser));
    check_text("sta");
    // skip "status"
    check(JSON_CONTINUE == jspp_skip_next(&parser));
    check(JSON_MEMBER_NAME == jspp_continue(&parser, json42, sizeof(json42) - 1));
    check_text("rc");
    check(JSON_INTEGER == jspp_next(&parser));
    check_text("97");
    check(JSON_OBJECT_END == jspp_skip_next(&parser));
    check(JSON_END == jspp_next(&parser));

    return 0;
}

int main()
{
    test(parse_simple_json, "Parse a one element JSON");
    test(parse_split_string, "Parse a string that spans several data transmission fragments");
    test(parse_split_null, "Parse null that is split in two halves");
    test(parse_invalid_elements, "Reject JSON with a single non-conforming element");
    test(parse_numbers, "Parse a single element JSON(s) with various (formats of) numbers");
    test(parse_split_numbers, "Parse numbers split between transmission fragments");
    test(parse_array, "Parse JSON arrays");
    test(parse_split_array, "Parse array that continues in another transmission fragment");
    test(parse_object, "Parse JSON objects");
    test(parse_split_object, "Parse object split between transmission fragments");
    test(skip_elements, "Skip JSON elements");
    printf("DONE: %d/%d\n", num_tests_passed, num_tests_passed + num_tests_failed);
    return num_tests_failed > 0;
}
