#pragma once

#include "mpeg_defs.h"

// @return 0 on success.
int parse_mpeg_file(const char *filename, mpeg_ctx_t *ctx, int is_ts);
