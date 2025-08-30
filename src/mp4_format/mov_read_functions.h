#pragma once

#include "mov_defs.h"

// @return 0 on success.
int parse_mov_file(const char *filename, mov_ctx_t *ctx);

