#pragma once
#include <stdio.h>
#include <stdint.h>

typedef struct flv_track_t {
    uint64_t id;

    int is_video;
    int is_audio;
    int is_subtitle;
} flv_track_t;

typedef struct flv_ctx_t {
    FILE *f;

    uint32_t tag_count;
} flv_ctx_t;

