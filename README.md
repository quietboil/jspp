# JSON Streaming Pull Parser

**jspp** is a C library for parsing JSON. It aims to provide memory constrained devices with means to parse potentially large JSON streams that might be even much larger than the data RAM those devices have.

## Motivation

One of my hobby projects - a smart light switch - needed a library to parse JSON responses that it gets from web services. This particular device is built around ESP8266. While that device itself has plenty of data memory (well, "plenty" in the world of SOCs :smiley:) two points made me somewhat sensitive regarding the amount that actually might be available to the application:
- The required modules reserve quite a bit of data RAM for themselves (just look at the size of .bss for instance). While that still leaves "a lot" :smiley: ...
- My prior experience with AVRs (mostly 328p) made me wary of memory allocation. Soon enough a new feature will need that memory and it might not be available if one is not careful.

I tried to find the existing library that would do the job. I had only 3 criteria in mind:
- It should be in C and ideally would not have external dependencies.
- It should not, if possible, allocate memory. At least not lot and, if it does, I'd like to have some control over the allocation.
- It should allow me to parse JSON fragments as they arrive and do not expect to have the entire JSON in memory before parsing it.

The last requirement probably was the most important (my memory anxiousness again :smiley:) as some services I considered during the design phase returned very large responses. Why anyone would need an antology in the web service response is still beyond me. Alas at least one service I wanted to use has it.

While I found quite a few libraries that parse JSON, some better than others in my opinion, I could not find one that would work for me. I'm sure it's out there, but after some time I just gave up and this JSON parser was born. Yep, yet another one :smiley:

## Features

The main features of this library are designed to make it effective in, maybe severely, constrained environments:

- **No memory allocation** - With the exception of automatic stack allocation by C for function arguments and variables *jspp* does not allocate memory at all - it does not have any static variables, it does not allocate any memory dynamically. It also does not *require* any memory allocation - static or dynamic - to be performed by the caller.
- **Streaming parsing** - A memory constrained device may not be able to receive the entire JSON message at once. *jspp* is able to parse each data fragment as it becomes available and continue from the point where it left off when the next fragment arrives.
- **Pull parsing** - The *jspp* API is designed for the *pull parsing* programming model - a library user calls parser functions when needed.
- **Filter unnecessary elements** - *jspp* allows the caller to skip elements in JSON reponse that the application is not interested in.

> **Note:** streaming parsing of the payload that is delivered in fragments often (maybe even always) means that one has to implement a state machine. The latter gives the abillity to track where one is in the parsing process and continue from the point where one was interrupted when the next fragment arrives. State machines often are generated. Manually coding them might not be your piece of cake. That's one of the drawbacks of this library. Check `sunrise-sunset` example (in `examples`) to get an idea of how a manually implemented machine might look like. Also review **jspp**'s complementary project - [**jssp**](/quietboil/jssp) - that generates state machines required for semi-automated data extraction.

These *jspp* features might also be interesting/important:

- **No dependencies** - The implementation is pure C that does not call any external functions. Not even libc.
- **[Tolerant Reader](https://martinfowler.com/bliki/TolerantReader.html)** - *jspp* is trying to be liberal in what it accepts as a payload.

> **Note:** JSON is a very simple format so there is not much wiggle room for being very tolerant. *jspp* enforces the basic JSON structure and rules (it has to otherwise it'll get lost in a badly formed document). Some elements however are only glanced at and accepted even though they might not strictly adhere to the JSON rules. The reasoning behind this (other than *it's much easier to implement* :smiley:) is that there is a possibility that the application may not even be interested in the badly formed item. As long as we can skim over it, we should be fine. The side effect of this however is that, if the application needs the very element that violated some JSON rule, then the application would have to be prepared to deal with it.
>
> There are 2 elements that *jspp* does not reject if it encounters them:
> - Numbers are allowed to *start* with 0, in other words not only "0" is a valid number, "000" will be accepted, as well as "00.00". The things to watch for, if that is the data element that the application needs, is that "089" will not be rejected either and then functions like `strtol` will not read the whole number.
> - Escape `\` in strings can escape anything. *jspp* does not check the escape rules specified for JSON (it only looks for and cares about the escaped `"`).

## Installation

Start by cloning or downloading and extracting the ZIP of this repository. For example:
```
$ git clone https://github.com/quietboil/jspp
```

Then execute `make` to build the library.

> **Note:** you might need to create `config.mk` file to define `CC`, `AR`, `CFLAGS`, `LDFLAGS`, etc. make variables if you are crosscompiling. As-is `make` will build the libary for your host.

Move `libjspp.a` to a suitable location for libraries and `jspp.h` for C headers that will be used to build your application.

You are all set. Optionally review `tests.c` for hints of usages.

> **Note:** if you are crosscompiling, create `config.mk` script and define the target compiler - `CC`, `CFLAGS`, `AR` - in that file.

## Example

Let's say the web service response is small enough to fit into a single data transmission fragment that the device can accept:
```json
{
    "target": "World",
    "action": "Hello"
}
```

Let's assume that the device OS provides:

- A function to send messages to tasks running on a device.
- A function to execute HTTP requests that accepts a URL and a callback function address.

Let's also assume that those 2 assumptions are defined in some `sys.h` file that our project will include:
```h
// ...
typedef void (*http_cb_t)(const uint8_t * http_data, uint16_t data_size);

void http_get(const char * url, http_cb_t cb);
void send_message_to_task(uint8_t task_id, uint16_t msg);

```

If you think that those two function do not look very real, you would be correct. They are contrived creatures with only one purpose - to help demo this example.

Somewhere in your program an HTTP request was executed:
```c
http_get("http://example.net/task?client=123", process_example_response);
```

And here's what might be happening in that reponse handler:
```c
#include <sys.h>
#include <string.h>
#include <jspp.h>

enum _targets {
    NOBODY,
    WORLD
};
enum _actions {
    DO_NOTHING,
    SAY_HELLO
};

// Let's say device init routine saved display task ID in this variable
extern uint8_t display_task_id;

void process_example_response(const uint8_t * http_data, uin16_t data_size)
{
    // Process headers
    // ...
    // Let's assume that at this point:
    //  - http_data pointer has moved to the beginning of the payload data and
    //  - data_size was modified to reflect payload size (either by reducing it during headers scan
    //    or by setting it from the Content-Type)
    // Process the payload
    const char * text;
    uint16_t     text_length;
    jasonspp_t   parser;
    
    if (JSON_OBJECT_BEGIN != jspp_start(&parser, http_data, data_size)) {
        // Unexpected payload
        return;
    }
    if (JSON_MEMBER_NAME token != jspp_next(&parser)) {
        // likely a bad JSON in which case token would be JSON_INVALID
        return;
    }

    text = jspp_text(&parser, &length);
    if (strncmp("target", text, text_length) != 0) {
        // Cannot recognize the returned object
        return;
    }
    if (JSON_STRING != jspp_next(&parser)) {
        // Unexpected data format. Maybe it's a new revision...
        // Anyway, we do understand it, so...
        return;
    }

    uint8_t target = NOBODY; // be lenient and use default if the target us unknown

    text = jspp_text(&parser, &length)
    if (strncmp("World", text, text_length) == 0) {
        target = WORLD;
    }
  
    if (JSON_MEMBER_NAME != jspp_next(&parser)) return;  // JSON structure issues
    text = jspp_text(&parser, &length);
    if (strncmp("action", text, text_length) != 0) return;  // Not what we expected
    if (JSON_STRING != jspp_next(&parser)) return;       // Unexpected data format

    uint8_t action = DO_NOTHING;

    text = jspp_text(&parser, &length)
    if (strncmp("Hello", text, text_length) == 0) {
        target = SAY_HELLO;
    }

    // at this point we have everything we need so we can just abandon the parser
    // or... if we are so inclined, we can see how it ends
    if (JSON_OBJECT_END != jspp_next(&parser)) return;
    if (JSON_END != jspp_next(&parser)) return;

    uintptr_t message = target << 8 | action;
    send_message_to_task(display_task_id, message);
}
```

And then that "display task" will react somehow to the message we sent. Maybe it'll print "Hello, World!" on the device display :smiley:

## API Reference

### Start

```h
uint8_t jspp_start(jspp_t * parser, const char * text, uint16_t text_len);
```
This function initializes the parser and returns the code of the very first token found in the data stream.

The posible token codes are enumerated in `jspp.h` in the `enum _json_tokens`. First four codes represent error and data processing conditions:
- `JSON_INVALID` is returned when JSON has a deficiency that prevents *jspp* from parsing it.
- `JSON_TOO_DEEP` is returned when JSON has too many levels of nested elements. By default it should be able to parse JSON with up to 12 levels of nested elements. If the failing JSON has more than 12 levels, then the value of `JSON_MAX_STACK` defined in `jspp.h` should be updated - it should be set to at least maximum possible nesting level + 2. **Note** that `libjspp.a` should be rebuilt after this change.
- `JSON_END` is returned when the entire JSON has been accepted. **Note** that *jspp* does not care if there are other data in the payload after JSON.
- `JSON_CONTINUE` is returned when the end of the current fragment is reached before JSON is completely accepted by the parser. The application would continue parsing JSON when the next fragment becomes available by executing `jspp_continue`.

In cases then data fragment ends somewhere in the middle of the JSON the last element may not be completely recognized. The follwoing 3 codes represent partially recognized JSON elements:
- `JSON_NUMBER_PART` is returned when *jspp* knows that it is looking at the number, but cannot be sure which one or whether it has seen all the digits of it when the fragment ended. When the number is fully recognized (after `jspp_continue` fed more data to the parser) a specific code will be returned for one of the 3 numeric types (see below). **Note** that it always returns *partial number* code even when it has a better idea about the number that is being parsed.
- `JSON_STRING_PART` is returned when fragment ended before terminating `"` was encountered. **Note** that when a string is especially long and fragments are relatively short this code might be returned multiple times. The calls to `jspp_continue` would immediately return this *partial string* code indicating that the entire fragment was in inner part of a longer string. When the final string fragment is scanned `jspp_continue` will return `JSON_STRING`.
- `JSON_MEMBER_NAME_PART` is returned when fragment is ended before `"` that terminates object member name is encountered. **Note** that for a typical JSON when more data become available `jspp_continue` will return `JSON_MEMBER_NAME` and then the entire member name could be retrieved via `jspp_text`. Theoretically it is possible of course that the member name can be very long. Then, with relatively short data fragments, a member name will behave like a string (member names are strings after all). In those cases *jspp* would return *partial member name* multiple times. This would be the most unsual in the real world though.

> **Note:** in all 3 cases of the *partially recognized elements* `jspp_text` will return currently recognized - partial - text of the token. It is the applicaiton's responsibility to either process it part by part (long strings for instance), or copy parts into a side-buffer and assemble those parts into a whole before processing them. For an example of how the latter can be achived see `sunrise-sunset.c` in `examples`.

The rest are codes that represent recognized JSON elements:
- `JSON_NULL` is returned when `null` is recognized. **Note** that `jspp_text` can stil be called if desired and it'll return the text of the token.
- `JSON_TRUE` is returned when `true` is recognized.
- `JSON_FALSE` is returned when `true` is recognized.
- `JSON_INTEGER` is returned when an integer like number (that has only digits) is recognized.
- `JSON_DECIMAL` is returned when a number has a decimal point but does not have an exponent. The distinction is made to support cases where it makes more sense to process them as fixed point instead of floating point numbers.
- `JSON_FLOATING_POINT` is returned when a number has an exponent.
- `JSON_STRING` is returned when a string is encountered.
- `JSON_MEMBER_NAME` is returned when a string is encountered in a position where JSON object member name is expected.
- `JSON_OBJECT_BEGIN` is returned when *jspp* recognized `{`
- `JSON_OBJECT_END` is returned when *jspp* recognized `}`
- `JSON_ARRAY_BEGIN` is returned when *jspp* recognized `[`
- `JSON_ARRAY_END` is returned when *jspp* recognized `]`

### Continue

```h
uint8_t jspp_continue(jspp_t * parser, const char * text, uint16_t text_len);
```
This functions makes parser continue parsing JSON after one of the partial token codes or `JSON_CONTINUE` was returned when additional data fragment becomes available. In general parsing JSON that is split in many fragments will require calling `jspp_start` and passing it the first fragment to start parsing and then invoke `jspp_continue` for each additional fragment.

`jspp_continue` like `jspp_start` returns the code of the first token it recognizes in the new fragment.

### Next

```h
uint8_t jspp_next(jspp_t * parser);
```
This function returns the nex token recognized in the current JSON fragment. `jspp_next` is called repeatedly, after processing is started by `jspp_start` or `jspp_continue`.

### Text

```h
const char * jspp_text(jspp_t * parser, uint16_t * length);
```
This function returns a pointer to the recognized token text and it saves the length of that text in the `length` variable which is passed via its pointer.

> **Note** that `jspp_text` returns a pointer to the text in the data fragment which *jspp* does not modify. Thus the token text is not `\0` terminated and the application must use length limited variants of string functions, i.e. `strncmp` vs `strcmp`, to process the token text.

### Skip

```h
uint8_t jspp_skip(jspp_t * parser);
```
This function skips the *current* JSON **element** and return the code of the token after the skipped element. Its behavior depends on what the current element (as indicated by the last token) is:

- When the current element is a *fully* recognized number (`JSON_INTEGER`, `JSON_DECIMAL` or `JSON_FLOATING_POINT`), a string (`JSON_STRING`) or an object member name (`JSON_MEMBER_NAME`) there is really nothing to skip, so the function behaves as if `jspp_next` has been called.
- When the current element is a partially recognized element - `JSON_NUMBER_PART`, `JSON_STRING_PART` or `JSON_MEMBER_NAME_PART` - this function will skip the rest of the element and return the token after the skipped one. Note that in this case - as a partial token can only be encountered at the end of the current data fragment - `jspp_skip` itself actually will return `JSON_CONTINUE`. However it also configures the parser to skip the rest of the current element when the next fragment is available. In other words when the `jspp_continue` is called it will know that it needs to skip the rest of the current element and return the token after the skipped one.
- When the current element is an object or an array (as indicated by the most recently returned `JSON_OBJECT_BEGIN` or `JSON_ARRAY_BEGIN`) `jspp_skip` will skip the entire object or the entire array.

The last behavior is where this function is the most useful. When parsing expects something in particular, but instead encounters a differently sttuctured JSON it has two options:
- abandon payload processing entirely and return error or
- skip the unexpected element and discover and return only the partual result.

> **Note** that like `jspp_skip_next` (see below) this function skips elements rather than tokens. It also (as `jspp_skip_next` does) might return `JSON_CONTINUE` if it is skipping over a particularly large element or when the element being skipped is interrupted by the end of the current text fragment.

### Skip Next

```h
uint8_t jspp_skip_next(jspp_t * parser);
```
This function behaves like `jspp_next` except it will first skip the next JSON **element** and return the code of the token after the skipped element.

> **Note** that this function does not skip tokens. It skips elements, which could actually be tokens - strings, numbers, etc. or entire objects or arrays if that's what was next in the stream. Objects and arrays can have nested elements of their own (up to the limit allowed by the `JSON_MAX_STACK`) that will also be skipped being a part of the object/array that is being skipped.

When `jspp_skip_next` is skipping particularly large element which crosses the edge of the current fragment, `jspp_skip` will actually return `JSON_CONTINUE` and will set a special condintion to let `jspp_continue` know that it should keep skipping tokens and return the token after the current element is completely skipped.

> **Note** that when the element crosses multiple data fragments, then `jspp_continue` will also return `JSON_CONTINUE`. This will continue until the current element is skipped. Then and only then the next token is returned.

## Tests

*jspp* unit tests are contained in a single executable that was built with the library. To run them execute:
```sh
tests
```
