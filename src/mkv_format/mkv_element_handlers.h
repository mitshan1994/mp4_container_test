#pragma once

#include "mkv_defs.h"

int ele_segment(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_segment_uuid(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_timestamp_scale(mkv_ctx_t *ctx, void *p, size_t data_len);

int ele_track_entry(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_track_number(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_track_codecid(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_track_type(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_track_codec_private(mkv_ctx_t *ctx, void *p, size_t data_len);

int ele_cluster(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_cluster_timestamp(mkv_ctx_t *ctx, void *p, size_t data_len);
int ele_simple_block(mkv_ctx_t *ctx, void *p, size_t data_len);

