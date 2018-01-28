# JSPP Examples

## Sunrise-Sunset

This example demos how one might build a state machine to extract required data elements from the JSON document. The demo appliction extract several data elements from the data returned by the [Sunrise-Sunset](https://sunrise-sunset.org/) web service. The JSON that the *sunrise-sunset* returns is quite simple, so the basic tecniques can be shown without (holefully) impeding the understanding of the implementation.

The example relies on a certain interaction with the networking layer of the underlying OS. To emulate the expected flow a small library `httpget` is included with this example. It provided only one function:
```h
typedef void (*http_get_cb_t)(const char * data, uint16_t size, void * cb_data);

int http_get(const char * host, const char * request, http_get_cb_t callback, void * callback_data);
```

However the assumed flow requires a caller to implement a callback that will be executed when a fragment of the HTTP response becomes available.

To ensure that even the small JSON from *sunrise-sunset* is split into fragments the library reads data into a relatively small buffer (256 bytes). This results in several callback (request data handler) executions.

The `httpget` callback - `handle_ws_response` in `sunrise-sunset.c` - is implemented as the state machine that allows it to be interrupted by the sudden and of the data at the end of the fragment and then be continue matching when the next fragment becomes available.

`sunrise-sunset.c` provides additional details and hints about the implementation and some reasons of choosing certain implementation strategies in the comments.

## Installation

To build examples execute:
```sh
make
```
