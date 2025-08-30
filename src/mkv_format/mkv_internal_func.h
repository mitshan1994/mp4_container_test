#pragma once
#include "read_utils.h"

// Wrapper functions for mkv_ctx_t
inline uint32_t read_int8_mkv(mkv_ctx_t *ctx) { return read_int8(ctx->f); }
inline uint32_t read_int16_mkv(mkv_ctx_t *ctx) { return read_int16(ctx->f); }
inline uint32_t read_int24_mkv(mkv_ctx_t *ctx) { return read_int24(ctx->f); }
inline uint32_t read_int32_mkv(mkv_ctx_t *ctx) { return read_int32(ctx->f); }
inline uint64_t read_int48_mkv(mkv_ctx_t *ctx) { return read_int48(ctx->f); }
inline uint64_t read_int64_mkv(mkv_ctx_t *ctx) { return read_int64(ctx->f); }
inline int skip_bytes_mkv(mkv_ctx_t *ctx, int64_t bytes) { return skip_bytes(ctx->f, bytes); }
inline int read_bytes_mkv(mkv_ctx_t *ctx, int64_t bytes, void *dst) { return read_bytes(ctx->f, bytes, dst); }
inline void back_bytes_mkv(mkv_ctx_t *ctx, int64_t bytes) { _fseeki64(ctx->f, -bytes, SEEK_CUR); }

const char *get_depth_space(uint32_t depth);

// read bytes from context and parse VINT.
int parse_VINT(mkv_ctx_t *ctx, uint64_t *v, uint8_t *used_len);

// parse VINT from given buffer.
int get_VINT(uint8_t *buf, uint64_t buf_len, uint64_t *v, uint8_t *used_len);

mkv_track_t *mkv_get_track_by_id(mkv_ctx_t *ctx, uint64_t trackid);
