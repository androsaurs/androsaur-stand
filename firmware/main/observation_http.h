#pragma once
#include <stdbool.h>
#include <stddef.h>

#define OBS_HEADER_MAX 1536
#define OBS_BODY_MAX 512
typedef enum { OBS_STATUS, OBS_TRACKING, OBS_BATTERY } observation_route;
typedef struct {
    observation_route route;
    size_t content_length;
    char token[65];
    bool close;
} observation_request;

// Parses one complete header in place. No allocation, I/O or credential logging.
bool observation_http_parse(char *header, size_t length, observation_request *out);
