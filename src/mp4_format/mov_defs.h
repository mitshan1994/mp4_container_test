#pragma once

#include <stdint.h>
#include <stdio.h>

#define MOV_BOX_TYPE(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))


typedef struct tag_mov_atom {
    uint32_t type;
    int64_t size;       // total size (excluding the size and type fields)

    char str_type[5];   // For debug purpose.
} mov_atom_t;

typedef struct tag_track {
    int valid;                  // Indicate a valid track.

    // tkhd
    uint32_t trackid;
    uint32_t width;
    uint32_t height;

    // mdhd
    uint64_t create_time;
    uint64_t modify_time;
    uint32_t timescale;
    uint64_t duration;

    // hdlr
    char handler_type[5];
    int is_video;
    int is_audio;

    // stsd
    char codec_format[5];       // SampleEntry format, 4 bytes.
    uint16_t data_ref_index;    // Used to get samples associated with this index.

    // stsd (video)
    char compressor_name[33];
    uint16_t depth;
    uint8_t length_size;        // lengthSizeMinusOne + 1

    // stsd (video) avc
    uint8_t *sps;
    uint32_t sps_len;
    uint8_t *pps;
    uint32_t pps_len;
    uint8_t *vps;               // hevc only.
    uint32_t vps_len;

    // stsd (audio)
    uint16_t channel_count;
    uint16_t audio_sample_size;
    uint32_t audio_sample_rate;

    // stts
    uint32_t stts_entry_count;
    uint32_t *stts_sample_counts;
    uint32_t *stts_sample_deltas;      // Delta of samples in the time-scale of the media

    // ctts
    uint32_t ctts_entry_count;
    uint32_t *ctts_sample_counts;
    uint32_t *ctts_sample_offsets;

    // stss
    uint32_t sample_number_count; // Count of "sample number"
    uint32_t *sample_numbers;   // Array of "sample number"

    // stsc
    uint32_t stsc_count;
    uint32_t *stsc_first_chunk;
    uint32_t *stsc_sample_per_chunk;
    uint32_t *stsc_sample_desc_index;

    // stsz
    uint32_t *sample_lengths;
    uint32_t sample_lengths_count;

    // stco
    uint64_t *chunk_offsets;
    uint32_t chunk_offset_count;

    // tfhd
    uint64_t cur_frag_offset;
    uint32_t cur_frag_default_sample_size;

    // trun
    uint32_t *trun_sample_sizes;
    uint64_t *trun_sample_offsets;
    uint32_t trun_sample_count;
    uint32_t trun_sample_capacity;

} mov_track_t;

typedef struct tag_mov_context {
    FILE *f;

    // mvhd
    uint64_t create_time;
    uint64_t modify_time;
    uint32_t timescale;
    uint64_t duration;

    mov_track_t *tracks;        // Since track id starts from 1, there could be empty track.
    int track_count;
    mov_track_t *cur_track;     // During parsing, points to current track.

    // moof parsing.
    uint64_t cur_moof_offset;   // Offset of the current moof box.
} mov_ctx_t;

typedef struct tag_mov_box_handler
{
    uint32_t type;
    int (*box_handler_func)(mov_ctx_t *ctx, mov_atom_t atom);
} mov_box_handler_t;
