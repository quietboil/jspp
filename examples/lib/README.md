# HTTP GET Library

This library provides means for the *jspp* examples to execute HTTP GET requests.

Currently it relies on the BSD (UNIX wold) and Windows sockets.

## Features

The library implements a flow where a caller is required to implement a callback:
```h
typedef void (*http_get_cb_t)(const char * data, uint16_t size, void * cb_data);
```

The library deliberately uses a small buffer to ensure that even short responses (unless they are extremely short) would be split in several fragments. This allows *jspp* demoes to show how JSON can be parsed fragment by fragment.

## API

The API consistns of only one function:
```h
int http_get(const char * host, const char * request, http_get_cb_t callback, void * callback_data);
```

This function will send HTTP GET request to the indicated web server and upon receiving response data will call specified callback repeatedly for each data fragment.

> **Note:** `http_get` uses hardcoded port 80 as this was sufficient for the current examples.

If the entire HTTP GET flow was successful `http_get` will return 0. Otherwise a positive number indicates a point where the flow was interrupted.

> See the implementation for meaning of speicifc error codes/interruption points.
