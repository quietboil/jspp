#ifndef __HTTPGET_H
#define __HTTPGET_H

#include <stdint.h>

typedef void (*http_get_cb_t)(const char * data, uint16_t size, void * cb_data);

int http_get(const char * host, const char * request, http_get_cb_t callback, void * callback_data);

#endif
