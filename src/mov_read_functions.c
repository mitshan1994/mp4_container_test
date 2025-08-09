#include "mov_read_functions.h"

#include <stdlib.h>
#include <string.h>

static mov_track_t *get_track_by_id(mov_ctx_t *ctx, uint32_t trackid);

// Read integer in network byte order.
static uint32_t read_int8(mov_ctx_t *ctx);
static uint32_t read_int16(mov_ctx_t *ctx);
static uint32_t read_int24(mov_ctx_t *ctx);
static uint32_t read_int32(mov_ctx_t *ctx);
static uint64_t read_int48(mov_ctx_t *ctx);
static uint64_t read_int64(mov_ctx_t *ctx);

// Skip given bytes in reading context.
// @return 0 on success.
static int skip_bytes(mov_ctx_t *ctx, int64_t bytes);
static int read_bytes(mov_ctx_t *ctx, int64_t bytes, void *dst);
static uint32_t read_box_type(mov_ctx_t *ctx);

// Read "type" and "size" part of a Box. "largesize" is handled.
static mov_atom_t read_box_atom_head(mov_ctx_t *ctx);

static const mov_box_handler_t *get_box_handler(uint32_t box_type);
static int parse_common_box(mov_ctx_t *ctx);
static int parse_sub_boxes(mov_ctx_t *ctx, mov_atom_t atom);

static int parse_moov_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_mvhd_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_trak_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_tkhd_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_mdia_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_mdhd_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_hdlr_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_minf_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stbl_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stsd_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stts_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_ctts_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stss_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stsc_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stsz_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_stco_box(mov_ctx_t *ctx, mov_atom_t atom);

static int parse_moof_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_mfhd_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_traf_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_tfhd_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_tfdt_box(mov_ctx_t *ctx, mov_atom_t atom);
static int parse_trun_box(mov_ctx_t *ctx, mov_atom_t atom);

static int parse_avcC_box(mov_ctx_t *ctx, mov_atom_t atom);  // avc
static int parse_hvcC_box(mov_ctx_t *ctx, mov_atom_t atom);  // hevc
static int parse_esds_box(mov_ctx_t *ctx, mov_atom_t atom);  // mp4a (aac)

static const mov_box_handler_t mov_box_handlers[] = {
    { MOV_BOX_TYPE('m','o','o','v'), parse_moov_box },
    { MOV_BOX_TYPE('m','v','h','d'), parse_mvhd_box },
    { MOV_BOX_TYPE('t','r','a','k'), parse_trak_box },
    { MOV_BOX_TYPE('t','k','h','d'), parse_tkhd_box },
    { MOV_BOX_TYPE('m','d','i','a'), parse_mdia_box },
    { MOV_BOX_TYPE('m','d','h','d'), parse_mdhd_box },
    { MOV_BOX_TYPE('h','d','l','r'), parse_hdlr_box },
    { MOV_BOX_TYPE('m','i','n','f'), parse_minf_box },
    { MOV_BOX_TYPE('s','t','b','l'), parse_stbl_box },
    { MOV_BOX_TYPE('s','t','s','d'), parse_stsd_box },
    { MOV_BOX_TYPE('s','t','t','s'), parse_stts_box },
    { MOV_BOX_TYPE('c','t','t','s'), parse_ctts_box },
    { MOV_BOX_TYPE('s','t','s','s'), parse_stss_box },
    { MOV_BOX_TYPE('s','t','s','c'), parse_stsc_box },
    { MOV_BOX_TYPE('s','t','s','z'), parse_stsz_box },
    { MOV_BOX_TYPE('s','t','c','o'), parse_stco_box },
    { MOV_BOX_TYPE('c','o','6','4'), parse_stco_box },

    { MOV_BOX_TYPE('m','o','o','f'), parse_moof_box },
    { MOV_BOX_TYPE('m','f','h','d'), parse_mfhd_box },
    { MOV_BOX_TYPE('t','r','a','f'), parse_traf_box },
    { MOV_BOX_TYPE('t','f','h','d'), parse_tfhd_box },
    { MOV_BOX_TYPE('t','f','d','t'), parse_tfdt_box },
    { MOV_BOX_TYPE('t','r','u','n'), parse_trun_box },

    { 0, NULL }  // end
};

void print_dump_data(const char *prefix, void *data, uint32_t bytes)
{
    printf(prefix);
    for (int i = 0; i != bytes; ++i) {
        printf(" %02X", *((uint8_t *)data + i));
    }
    printf("\n");
}

int parse_mov_file(const char *filename, mov_ctx_t *ctx)
{
    int ret;

    if (ctx->f != NULL) {
        printf("file stream is not null\n");
        return -1;
    }

    ctx->f = fopen(filename, "rb");
    if (!ctx->f) {
        printf("failed to open file: %s\n", filename);
        return -1;
    }

    // Get file size.
    int64_t file_size;
    _fseeki64(ctx->f, 0, SEEK_END);
    file_size = _ftelli64(ctx->f);
    _fseeki64(ctx->f, 0, SEEK_SET);

    while ((ret = parse_common_box(ctx)) == 0) {
        // Check for file end.
        int64_t cur_file_pos = _ftelli64(ctx->f);
        if (cur_file_pos + 8 > file_size) {
            printf("file end reached. file size: %lld, cur pos: %lld\n", file_size, cur_file_pos);
            break;
        }
    }

    return 0;
}

static mov_track_t *get_track_by_id(mov_ctx_t *ctx, uint32_t trackid)
{
    for (int i = 0; i != ctx->track_count; ++i) {
        if (ctx->tracks[i].trackid == trackid) {
            return &ctx->tracks[i];
        }
    }
    return NULL;
}

static uint32_t read_box_type(mov_ctx_t *ctx)
{
    uint32_t type;
    int ret = (int)fread(&type, sizeof(type), 1, ctx->f);
    if (ret != 1) {
        printf("failed to read box type. ret: %d\n", ret);
        return 0;
    }
    return type;
}

static mov_atom_t read_box_atom_head(mov_ctx_t *ctx)
{
    mov_atom_t atom;
    uint32_t size;

    size = read_int32(ctx);
    atom.type = read_box_type(ctx);
    memcpy(atom.str_type, &atom.type, 4);
    atom.str_type[4] = '\0';

    if (size == 1) {
        atom.size = read_int64(ctx);
        atom.size -= 16;
    } else {
        atom.size = size;
        atom.size -= 8;
    }

    return atom;
}

static const mov_box_handler_t *get_box_handler(uint32_t box_type)
{
    for (int i = 0; mov_box_handlers[i].type; ++i) {
        if (mov_box_handlers[i].type == box_type) {
            return mov_box_handlers + i;
        }
    }
    return NULL;
}

static int parse_common_box(mov_ctx_t *ctx)
{
    int ret;
    mov_atom_t atom = read_box_atom_head(ctx);

    printf("box encountered: %s\n", atom.str_type);
    //printf("  box start file pos: %lld\n", start_pos);

    int64_t content_start_pos = _ftelli64(ctx->f);

    printf("  box size: %lld\n", atom.size);

    // Find parse function for current box.
    const mov_box_handler_t *box_handler = get_box_handler(atom.type);
    if (box_handler) {
        ret = box_handler->box_handler_func(ctx, atom);
    } else {
        // Skip this box.
        ret = skip_bytes(ctx, atom.size);
        if (ret != 0) {
            printf("skip failed. ret: %d\n", ret);
        }
        printf("  mov box skipped: %s\n", atom.str_type);
    }

    int64_t cur_pos = _ftelli64(ctx->f);
    if (cur_pos - content_start_pos != atom.size) {
        printf("  box parsing incomplete! type: %s, remaining size: %lld. Seek forcely.\n",
            atom.str_type, content_start_pos + atom.size - cur_pos);
        _fseeki64(ctx->f, content_start_pos + atom.size, SEEK_SET);
    }

    //printf("  current file pos: %lld\n", _ftelli64(ctx->f));

    return ret;
}

static int parse_sub_boxes(mov_ctx_t *ctx, mov_atom_t atom)
{
    int ret;
    int64_t end_pos = _ftelli64(ctx->f) + atom.size;
    while (_ftelli64(ctx->f) < end_pos) {
        ret = parse_common_box(ctx);
        if (ret != 0) {
            printf("parse_sub_boxes failed.\n");
            return ret;
        }
    }
    return 0;
}

static int parse_moov_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    //printf("start parsing 'moov'\n");
    return parse_sub_boxes(ctx, atom);
}

static int parse_mvhd_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    int ret = 0;

    uint8_t version;
    uint32_t flags;
    version = read_int8(ctx);
    flags = read_int24(ctx);

    uint64_t create_time;
    uint64_t modify_time;
    uint32_t timescale;
    uint64_t duration;
    if (1 == version) {
        create_time = read_int64(ctx);
        modify_time = read_int64(ctx);
        timescale = read_int32(ctx);
        duration = read_int64(ctx);
    } else {
        create_time = read_int32(ctx);
        modify_time = read_int32(ctx);
        timescale = read_int32(ctx);
        duration = read_int32(ctx);
    }

    ctx->create_time = create_time;
    ctx->modify_time = modify_time;
    ctx->timescale = timescale;
    ctx->duration = duration;

    // Other fields omitted.
    skip_bytes(ctx, 80);

    printf("  create time: %llu, modify fime: %llu, timescale: %u, duration: %llu\n",
        create_time, modify_time, timescale, duration);

    return 0;
}

static int parse_trak_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    //printf("start parsing 'trak'\n");
    int ret = parse_sub_boxes(ctx, atom);

    // Unset current track pointer.
    ctx->cur_track = NULL;
    return ret;
}

static int parse_tkhd_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    int ret = 0;
    uint8_t version;
    uint32_t flags;

    version = read_int8(ctx);
    flags = read_int24(ctx);
    printf("  tkhd version: %d, flags: %d\n", version, flags);

    uint64_t create_time;
    uint64_t modify_time;
    uint32_t trackid;
    uint64_t duration;

    if (version == 1) {
        create_time = read_int64(ctx);
        modify_time = read_int64(ctx);
        trackid = read_int32(ctx);
        read_int32(ctx);    // reserved
        duration = read_int64(ctx);
    } else {  // version == 0
        create_time = read_int32(ctx);
        modify_time = read_int32(ctx);
        trackid = read_int32(ctx);
        read_int32(ctx);    // reserved
        duration = read_int32(ctx);
    }

    // reserved.
    read_int32(ctx);
    read_int32(ctx);

    uint16_t layer = read_int16(ctx);
    uint16_t alternative_group = read_int16(ctx);
    uint16_t volume = read_int16(ctx);
    read_int16(ctx);        // reserved

    // skip matrix
    for (int i = 0; i != 9; ++i) {
        read_int32(ctx);
    }

    uint32_t width = read_int32(ctx);
    uint32_t height = read_int32(ctx);

    width /= 1 << 16;
    height /= 1 << 16;

    printf("  width: %d, height: %d\n", width, height);

    // Create new track.
    if (ctx->track_count <= (int)trackid) {
        void *old_tracks = ctx->tracks;
        int old_count = ctx->track_count;

        ctx->tracks = calloc(trackid + 1, sizeof(mov_track_t));
        ctx->track_count = trackid + 1;
        memset(ctx->tracks, 0, ctx->track_count * sizeof(mov_track_t));
        memcpy(ctx->tracks, old_tracks, old_count * sizeof(mov_track_t));
        free(old_tracks);
    }
    ctx->cur_track = ctx->tracks + trackid;
    ctx->cur_track->valid = 1;
    ctx->cur_track->trackid = trackid;
    ctx->cur_track->width = width;
    ctx->cur_track->height = height;

    return ret;
}

static int parse_mdia_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    printf("start parsing 'mdia'\n");
    return parse_sub_boxes(ctx, atom);
}

static int parse_mdhd_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    uint8_t version = read_int8(ctx);
    uint8_t flags = read_int24(ctx);

    uint64_t create_time;
    uint64_t modify_time;
    uint32_t timescale;
    uint64_t duration;
    if (1 == version) {
        create_time = read_int64(ctx);
        modify_time = read_int64(ctx);
        timescale = read_int32(ctx);
        duration = read_int64(ctx);
    } else {
        create_time = read_int32(ctx);
        modify_time = read_int32(ctx);
        timescale = read_int32(ctx);
        duration = read_int32(ctx);
    }

    cur_track->create_time = create_time;
    cur_track->modify_time = modify_time;
    cur_track->timescale = timescale;
    cur_track->duration = duration;

    // Other fields omitted.
    skip_bytes(ctx, 4);

    printf("  create time: %llu, modify fime: %llu, timescale: %u, duration: %llu\n",
        create_time, modify_time, timescale, duration);

    return 0;
}

static int parse_hdlr_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    //printf("start parsing 'hdlr'\n");

    if (NULL == ctx->cur_track) {
        printf("  NOT in track parsing!\n");
        return -1;
    }

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    int ret = 0;
    uint32_t pre_defined = read_int32(ctx);
    ret = read_bytes(ctx, 4, ctx->cur_track->handler_type);
    ctx->cur_track->handler_type[4] = '\0';
    if (ret != 0) {
        printf("  failed to read 'handler_type'\n");
        return -1;
    }

    // reserved.
    read_int32(ctx);
    read_int32(ctx);
    read_int32(ctx);

    // a variable-length "name". We don't read it here.
    if (atom.size > 6 * sizeof(int32_t)) {
        skip_bytes(ctx, atom.size - 6 * sizeof(int32_t));
    }

    printf("  track handler type: %s (***)\n", ctx->cur_track->handler_type);

    if (strncmp("vide", ctx->cur_track->handler_type, 4) == 0) {
        ctx->cur_track->is_video = 1;
    } else if (strncmp("soun", ctx->cur_track->handler_type, 4) == 0) {
        ctx->cur_track->is_audio = 1;
    }

    return 0;
}

static int parse_minf_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    //printf("start parsing 'minf'\n");
    return parse_sub_boxes(ctx, atom);
}

static int parse_stbl_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    //printf("start parsing 'minf'\n");
    return parse_sub_boxes(ctx, atom);
}

static int parse_stsd_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    int ret = 0;

    if (NULL == ctx->cur_track) {
        printf("  NO track is current!\n");
        return -1;
    }
    mov_track_t *cur_track = ctx->cur_track;

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t entry_count = read_int32(ctx);

    for (int i = 0; i != entry_count; ++i) {
        if (cur_track->is_video) {
            mov_atom_t video_entry_atom = read_box_atom_head(ctx);
            memcpy(cur_track->codec_format, &video_entry_atom.type, 4);
            cur_track->codec_format[4] = '\0';

            // SampleEntry
            skip_bytes(ctx, 6 * sizeof(uint8_t));
            uint16_t data_ref_index = read_int16(ctx);

            // VisualSampleEntry
            read_int16(ctx);    // pre_defined
            read_int16(ctx);    // reserved
            skip_bytes(ctx, 3 * sizeof(uint32_t));  // pre_defined
            uint16_t width = read_int16(ctx);
            uint16_t height = read_int16(ctx);
            read_int32(ctx);    // horiz resolution
            read_int32(ctx);    // verti resolution
            read_int32(ctx);    // reserved
            read_int16(ctx);    // frame count

            read_bytes(ctx, 32, cur_track->compressor_name);
            cur_track->compressor_name[32] = '\0';

            uint16_t depth = read_int16(ctx);
            read_int16(ctx);    // pre-defined

            // ISO/IEC 14496-15
            // read avc configuration.
            if (strncmp(cur_track->codec_format, "avc1", 4) == 0) {
                // read "avcC" Box
                mov_atom_t avcC_atom = read_box_atom_head(ctx);
                if (strncmp("avcC", avcC_atom.str_type, 4) == 0) {
                    ret = parse_avcC_box(ctx, avcC_atom);
                } else {
                    printf("  NOT 'avcC' box!\n");
                }
                
                // maybe skip the rest. depends on outer box seek.
                printf("  may have remaining data not parsed. currently skipping.\n");
            } else if (strncmp(cur_track->codec_format, "hvc1", 4) == 0) {
                mov_atom_t hevC_atom = read_box_atom_head(ctx);
                printf("  hvc1 sub-box type: %s\n", hevC_atom.str_type);

                if (strncmp("hvcC", hevC_atom.str_type, 4) == 0) {
                    ret = parse_hvcC_box(ctx, hevC_atom);
                } else {
                    printf("  NOT 'avcC' box!\n");
                }
            } else {
                printf("  currently not supported. video codec format: %s\n",
                    cur_track->codec_format);
            }
        } else if (cur_track->is_audio) {
            mov_atom_t audio_entry_atom = read_box_atom_head(ctx);
            memcpy(cur_track->codec_format, &audio_entry_atom.type, 4);
            cur_track->codec_format[4] = '\0';

            // SampleEntry
            skip_bytes(ctx, 6 * sizeof(uint8_t));
            uint16_t data_ref_index = read_int16(ctx);

            // AudioSampleEntry
            skip_bytes(ctx, 2 * sizeof(uint32_t));
            uint16_t channel_count = read_int16(ctx);
            uint16_t sample_size = read_int16(ctx);
            read_int16(ctx);    // predefined
            read_int16(ctx);    // reserved
            uint32_t sample_rate = read_int32(ctx) >> 16;

            cur_track->channel_count = channel_count;
            cur_track->audio_sample_size = sample_size;
            cur_track->audio_sample_rate = sample_rate;

            // ISO/IEC 14496-14
            if (strncmp("mp4a", cur_track->codec_format, 4) == 0) {
                mov_atom_t esds_atom = read_box_atom_head(ctx);
                if (strncmp("esds", esds_atom.str_type, 4) == 0) {
                    ret = parse_esds_box(ctx, esds_atom);
                    if (ret != 0) {
                        printf(" failed to parse 'esds' box");
                    }
                } else {
                    printf("  unrecognized sub-box of 'mp4a'\n");
                }
            }

            printf("  audio sample size: %hu, sample rate: %hu, channel_count: %hu\n",
                cur_track->audio_sample_size, cur_track->audio_sample_rate,
                cur_track->channel_count);
        }
    }

    return 0;
}

static int parse_stts_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t entry_count = read_int32(ctx);
    cur_track->stts_entry_count = entry_count;
    cur_track->stts_sample_counts = calloc(entry_count, sizeof(uint32_t));
    cur_track->stts_sample_deltas = calloc(entry_count, sizeof(uint32_t));

    for (int i = 0; i != entry_count; ++i) {
        uint32_t sample_count = read_int32(ctx);
        uint32_t sample_delta = read_int32(ctx);

        cur_track->stts_sample_counts[i] = sample_count;
        cur_track->stts_sample_deltas[i] = sample_delta;
    }

    printf("  stts entry_count: %u\n", entry_count);
    for (int i = 0; i < 10 && i != ctx->cur_track->stts_entry_count; ++i) {
        printf("    i=%d, stts sample count: %u, decoding delta: %u\n", i,
            cur_track->stts_sample_counts[i], cur_track->stts_sample_deltas[i]);
    }

    return 0;
}

// Composition Time to Sample Box
static int parse_ctts_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t entry_count = read_int32(ctx);

    cur_track->ctts_entry_count = entry_count;
    cur_track->ctts_sample_counts = calloc(entry_count, sizeof(uint32_t));
    cur_track->ctts_sample_offsets = calloc(entry_count, sizeof(uint32_t));

    for (int i = 0; i != entry_count; ++i) {
        uint32_t sample_count = read_int32(ctx);
        uint32_t sample_offset = read_int32(ctx);

        cur_track->ctts_sample_counts[i] = sample_count;
        cur_track->ctts_sample_offsets[i] = sample_offset;
    }

    printf("  ctts entry count: %u\n", entry_count);
    for (int i = 0; i < 10 && i != entry_count; ++i) {
        printf("  i=%d, ctts sample count: %u, offset: %u\n", i,
            cur_track->ctts_sample_counts[i], cur_track->ctts_sample_offsets[i]);
    }

    return 0;
}

static int parse_stss_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t entry_count = read_int32(ctx);
    ctx->cur_track->sample_number_count = entry_count;
    ctx->cur_track->sample_numbers = calloc(entry_count, sizeof(uint32_t));

    for (int i = 0; i != entry_count; ++i) {
        uint32_t sample_number = read_int32(ctx);
        ctx->cur_track->sample_numbers[i] = sample_number;

        //printf("  stss sample number: %u\n", sample_number);
    }

    printf("  stss sample number count: %u\n", entry_count);

    return 0;
}

// Sample to Chunk Box
static int parse_stsc_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t entry_count = read_int32(ctx);
    cur_track->stsc_count = entry_count;
    cur_track->stsc_first_chunk = calloc(entry_count, sizeof(uint32_t));
    cur_track->stsc_sample_per_chunk = calloc(entry_count, sizeof(uint32_t));
    cur_track->stsc_sample_desc_index = calloc(entry_count, sizeof(uint32_t));

    for (int i = 0; i != entry_count; ++i) {
        uint32_t first_chunk = read_int32(ctx);
        uint32_t samples_per_chunk = read_int32(ctx);
        uint32_t sample_desc_index = read_int32(ctx);

        cur_track->stsc_first_chunk[i] = first_chunk;
        cur_track->stsc_sample_per_chunk[i] = samples_per_chunk;
        cur_track->stsc_sample_desc_index[i] = sample_desc_index;
    }

    printf("  stsc (Sample to Chunk) entry_count: %u (***)\n", entry_count);
    // Print first 10.
    for (int i = 0; i < 10 && i != entry_count; ++i) {
        printf("  i=%d, first_chunk: %u, samples_per_chunk: %u, desc_index: %u\n",
            i, cur_track->stsc_first_chunk[i], cur_track->stsc_sample_per_chunk[i], 
            cur_track->stsc_sample_desc_index[i]);
    }

    return 0;
}

// Sample Size Box
static int parse_stsz_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t sample_size = read_int32(ctx);
    uint32_t sample_count = read_int32(ctx);

    // Allocate sample length array.
    cur_track->sample_lengths = calloc(sample_count, sizeof(uint32_t));
    cur_track->sample_lengths_count = sample_count;

    if (0 == sample_size) {
        for (int i = 0; i != sample_count; ++i) {
            uint32_t entry_size = read_int32(ctx);
            cur_track->sample_lengths[i] = entry_size;

            //printf("  stsz, i=%d, entry_size=%u\n", i, entry_size);
        }
    } else {
        for (int i = 0; i != sample_count; ++i) {
            cur_track->sample_lengths[i] = sample_size;
        }
    }

    printf("  stsz (Sample Size) sample count: %u (***)\n", sample_count);

    // Print first 10.
    for (int i = 0; i != sample_count && i < 10; ++i) {
        printf("  %d, sample length: %u\n", i, cur_track->sample_lengths[i]);
    }

    return 0;
}

// Chunk Offset Box
static int parse_stco_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    int use_large_offset = 0;
    if (strncmp(atom.str_type, "co64", 4) == 0) {
        use_large_offset = 1;
    }

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t entry_count = read_int32(ctx);
    cur_track->chunk_offsets = calloc(entry_count, sizeof(uint64_t));
    cur_track->chunk_offset_count = entry_count;

    for (int i = 0; i != entry_count; ++i) {
        uint64_t offset;
        if (use_large_offset) {
            offset = read_int64(ctx);
        } else {
            offset = read_int32(ctx);
        }
        cur_track->chunk_offsets[i] = offset;
    }

    printf("  %s (Chunk Offset) entry_count: %u (***)\n", atom.str_type, entry_count);

    // Print first 10.
    for (int i = 0; i != entry_count && i < 10; ++i) {
        printf("  %d, offset: %llu\n", i, cur_track->chunk_offsets[i]);
    }

    return 0;
}

static int parse_moof_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    ctx->cur_moof_offset = _ftelli64(ctx->f) - 8;  // Normal atom header is assumed.
    return parse_sub_boxes(ctx, atom);
}

static int parse_mfhd_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    uint32_t seq_number = read_int32(ctx);
    printf("  sequence number: %u\n", seq_number);

    return 0;
}

static int parse_traf_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    return parse_sub_boxes(ctx, atom);
}

static int parse_tfhd_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    uint64_t data_offset = ctx->cur_moof_offset;

    read_int8(ctx);     // version
    int tf_flags = read_int24(ctx);    // flags

    uint32_t trackid = read_int32(ctx);
    uint64_t base_data_offset;
    uint32_t sample_desc_index;
    uint32_t default_sample_duration;
    uint32_t default_sample_size;
    uint32_t default_sample_flags;

    if (tf_flags & 0x000001) {
        base_data_offset = read_int64(ctx);
        data_offset = base_data_offset;
    } else {
        base_data_offset = 0;
        printf("  base_offset is not present.\n");
    }

    if (tf_flags & 0x000002) {
        sample_desc_index = read_int32(ctx);
        printf("  sample_desc_index is: %u\n", sample_desc_index);
    } else {
        sample_desc_index = 0;
        printf("  sample_desc_index is not present\n");
    }

    if (tf_flags & 0x000008) {
        default_sample_duration = read_int32(ctx);
        printf("  default_sample_duration is %u\n", default_sample_duration);
    } else {
        default_sample_duration = 0;
        printf("  default_sample_duration is not present\n");
    }

    if (tf_flags & 0x000010) {
        default_sample_size = read_int32(ctx);
        printf("  default_sample_size is %u\n", default_sample_size);
    } else {
        default_sample_size = 0;
        printf("  default_sample_size is not present\n");
    }

    if (tf_flags & 0x000020) {
        default_sample_flags = read_int32(ctx);
        printf("  default_sample_flags is %u\n", default_sample_flags);
    } else {
        default_sample_flags = 0;
        printf("  default_sample_flags is not present\n");
    }

    printf("  base_data_offset: %llu\n", base_data_offset);

    mov_track_t *cur_track = get_track_by_id(ctx, trackid);
    if (cur_track && cur_track->is_video) {
        printf("  (video track)\n");
    } else if (cur_track && cur_track->is_audio) {
        printf("  (audio track)\n");
    }

    // Update "current track" for later process inside traf.
    ctx->cur_track = cur_track;

    cur_track->cur_frag_offset = data_offset;
    cur_track->cur_frag_default_sample_size = default_sample_size;

    return 0;
}

static int parse_tfdt_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    uint8_t version = read_int8(ctx);
    read_int24(ctx);

    uint64_t base_decode_time;
    if (1 == version) {
        base_decode_time = read_int64(ctx);
    } else {
        base_decode_time = read_int32(ctx);
    }

    printf("  base media decode time: %llu\n", base_decode_time);

    return 0;
}

static int parse_trun_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    mov_track_t *cur_track = ctx->cur_track;

    uint8_t version = read_int8(ctx);
    uint32_t tr_flags = read_int24(ctx);

    uint32_t sample_count = read_int32(ctx);
    int32_t data_offset;
    uint32_t first_sample_flags;
    if (tr_flags & 0x000001) {
        data_offset = read_int32(ctx) + cur_track->cur_frag_offset;
    } else {
        data_offset = cur_track->cur_frag_offset;
        printf("  data_offset is not present\n");
    }

    if (tr_flags & 0x000004) {
        first_sample_flags = read_int32(ctx);
    } else {
        printf("  first_sample_flags is not present\n");
    }

    int have_duration = 0;
    int have_size = 0;
    int have_flags = 0;
    int have_ct_offset = 0;
    if (tr_flags & 0x000100) {
        have_duration = 1;
    } else {
        printf("  sample_duration is not present\n");
    }
    if (tr_flags & 0x000200) {
        have_size = 1;
    } else {
        printf("  sample_size is not present\n");
    }
    if (tr_flags & 0x000400) {
        have_flags = 1;
    } else {
        printf("  sample_flags is not present\n");
    }
    if (tr_flags & 0x000800) {
        have_ct_offset = 1;
    } else {
        printf("  sample_composition_time_offset is not present\n");
    }

    // Allocate for new samples.
    if (cur_track->trun_sample_capacity == 0) {
        cur_track->trun_sample_sizes = calloc(sample_count, sizeof(uint32_t));
        cur_track->trun_sample_offsets = calloc(sample_count, sizeof(uint64_t));
        cur_track->trun_sample_capacity = sample_count;
    } else {
        uint32_t suitable_capacity = cur_track->trun_sample_capacity;
        while (suitable_capacity < cur_track->trun_sample_count + sample_count) {
            suitable_capacity = 2 * suitable_capacity;
        }
        if (suitable_capacity != cur_track->trun_sample_capacity) {
            cur_track->trun_sample_sizes =
                realloc(cur_track->trun_sample_sizes, suitable_capacity * sizeof(uint32_t));
            cur_track->trun_sample_offsets =
                realloc(cur_track->trun_sample_offsets, suitable_capacity * sizeof(uint64_t));
            cur_track->trun_sample_capacity = suitable_capacity;
        }
    }

    uint32_t sample_duration;
    uint32_t sample_size;
    uint32_t sample_flags;
    uint32_t sample_composition_time_offset;
    uint64_t cur_sample_offset = data_offset;
    for (int i = 0; i != sample_count; ++i) {
        if (have_duration) sample_duration = read_int32(ctx);
        if (have_size) {
            sample_size = read_int32(ctx);
        } else {
            sample_size = cur_track->cur_frag_default_sample_size;
        }
        if (have_flags) sample_flags = read_int32(ctx);
        if (have_ct_offset) sample_composition_time_offset = read_int32(ctx);

        cur_track->trun_sample_sizes[cur_track->trun_sample_count] = sample_size;
        cur_track->trun_sample_offsets[cur_track->trun_sample_count] = cur_sample_offset;

        cur_track->trun_sample_count++;
        cur_sample_offset += sample_size;
    }
    printf("  sample count: %u\n", sample_count);

    return 0;
}

static int parse_avcC_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    // AVCDecoderConfigurationRecord()

    mov_track_t *cur_track = ctx->cur_track;

    uint8_t config_version = read_int8(ctx);
    uint8_t profile_indication = read_int8(ctx);
    uint8_t profile_compat = read_int8(ctx);
    uint8_t level_indication = read_int8(ctx);

    uint8_t b = read_int8(ctx);
    if ((b >> 2) != 0x3F) {
        printf("  AVCDecoderConfigurationRecord bits not '111111' before lengthSizeMinusOne\n");
    }
    cur_track->length_size = (b & 0x3) + 1;

    b = read_int8(ctx);
    if ((b >> 5) != 0x7) {
        printf("  AVCDecoderConfigurationRecord bits not '111' before numOfSequenceParameterSets\n");
    }
    uint8_t num_sps = (b & 0x1F);
    for (int i = 0; i != num_sps; ++i) {
        uint16_t sps_len = read_int16(ctx);
        // Only read first sps.
        if (i == 0) {
            cur_track->sps = malloc(sps_len);
            read_bytes(ctx, sps_len, cur_track->sps);
            cur_track->sps_len = sps_len;
        } else {
            skip_bytes(ctx, sps_len);
        }
    }

    uint8_t num_pps = read_int8(ctx);
    for (int i = 0; i != num_pps; ++i) {
        uint16_t pps_len = read_int16(ctx);
        // Only read first pps.
        if (i == 0) {
            cur_track->pps = malloc(pps_len);
            read_bytes(ctx, pps_len, cur_track->pps);
            cur_track->pps_len = pps_len;
        } else {
            skip_bytes(ctx, num_pps);
        }
    }

    if (cur_track->sps_len) {
        print_dump_data("sps:", cur_track->sps, cur_track->sps_len);
    }
    if (cur_track->pps_len) {
        print_dump_data("pps:", cur_track->pps, cur_track->pps_len);
    }

    return 0;
}

static int parse_hvcC_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    uint8_t b;
    mov_track_t *cur_track = ctx->cur_track;

    uint8_t config_version = read_int8(ctx);

    // general_profile_space, general_tier_flag, general_profile_idc;
    b = read_int8(ctx);
    uint8_t profile_space = b >> 6;
    uint8_t tier_flag = (b >> 1) & 0x1;
    uint8_t profile_idc = b & 0x3;

    uint32_t profile_compat_flags = read_int32(ctx);
    uint64_t contraint_indicator_flags = read_int48(ctx);
    uint8_t level_idc = read_int8(ctx);

    // reserved 4 bits
    b = read_int8(ctx);
    if (b >> 4 != 0xF) {
        printf("  [error] reserved is not '0b 1111'\n");
    }
    uint16_t min_spatial_seg_idc = ((b & 0xF) << 8) | read_int8(ctx);

    b = read_int8(ctx);
    if (b >> 2 != 0x3F) {
        printf("  [error] reserved2 is not '0b 111111'\n");
    }
    uint8_t parallel_type = b & 0x3;

    b = read_int8(ctx);
    if (b >> 2 != 0x3F) {
        printf("  [error] reserved3 is not '0b 111111'\n");
    }
    uint8_t chroma_format_idc = b & 0x3;

    b = read_int8(ctx);
    if (b >> 3 != 0x1F) {
        printf("  [error] reserved4 is not '0b 11111'\n");
    }
    uint8_t bit_depth_luma_minus8 = b & 0x7;

    b = read_int8(ctx);
    if (b >> 3 != 0x1F) {
        printf("  [error] reserved4 is not '0b 11111'\n");
    }
    uint8_t bit_depth_chroma_minus8 = b & 0x7;

    uint16_t avg_frame_rate = read_int16(ctx);

    b = read_int8(ctx);
    uint8_t constant_frame_rate = b >> 6;
    uint8_t num_temp_layers = (b >> 3) & 0x7;
    uint8_t temp_id_nested = (b >> 2) & 0x1;
    uint8_t length_minus_one = b & 0x3;

    uint8_t num_arrays = read_int8(ctx);
    for (int i = 0; i != num_arrays; ++i) {
        b = read_int8(ctx);
        uint8_t array_completeness = b >> 7;
        uint8_t nalu_type = b & 0x3F;
        uint16_t num_nalus = read_int16(ctx);
        for (int j = 0; j != num_nalus; ++j) {
            uint16_t nalu_length = read_int16(ctx);

            uint8_t **ppData = NULL;
            uint32_t *pLen = NULL;

            if (32 == nalu_type) {
                ppData = &cur_track->vps;
                pLen = &cur_track->vps_len;
                printf("  VPS. length: %hu\n", nalu_length);
            } else if (33 == nalu_type) {
                ppData = &cur_track->sps;
                pLen = &cur_track->sps_len;
                printf("  SPS. length: %hu\n", nalu_length);
            } else if (34 == nalu_type) {
                ppData = &cur_track->pps;
                pLen = &cur_track->pps_len;
                printf("  PPS. length: %hu\n", nalu_length);
            } else {
                printf("  invalid nalu type when parsing hvcC: %hhu\n", nalu_type);
                skip_bytes(ctx, nalu_length);
            }

            if (NULL == *ppData) {
                *ppData = malloc(nalu_length);
                *pLen = nalu_length;
                read_bytes(ctx, nalu_length, *ppData);
                print_dump_data("    ", *ppData, nalu_length);
            }
        }
    }

    return 0;
}

static int parse_esds_box(mov_ctx_t *ctx, mov_atom_t atom)
{
    // ESDBox for mp4a(aac)

    uint8_t byte;

    read_int8(ctx);     // version
    read_int24(ctx);    // flags

    // ES_Descriptor. Refer to ISO/IEC 14496-1.
    uint8_t tag = read_int8(ctx);

    if (tag != 0x03) {
        printf("  esds not-processed tag: %u\n", tag);
        return 0;
    }
    printf("  esds first tag is 0x03 (ES_DescrTag)\n");
    // 0x03: ES_DescrTag for ES_Descriptor

    uint16_t es_id = read_int16(ctx);
    byte = read_int8(ctx);
    uint8_t stream_depend_flag = byte >> 7;
    uint8_t url_flag = (byte >> 6) & 0x1;
    uint8_t ocr_stream_flag = (byte >> 5) & 0x1;
    uint8_t stream_priority = byte & 0x1F;

    printf("  ES_Descriptor is not fully parsed now.\n");
    if (atom.size > 4) {
        skip_bytes(ctx, atom.size - 4);
    }

    return 0;
}

static uint32_t read_int8(mov_ctx_t *ctx)
{
    uint8_t v;
    size_t ret = fread(&v, sizeof(v), 1, ctx->f);
    if (ret != 1) {
        printf("failed to read int8\n");
        return 0;
    }
    return v;
}

static uint32_t read_int16(mov_ctx_t *ctx)
{
    uint32_t v;
    v = read_int8(ctx) << 8;
    v = read_int8(ctx) | v;
    return v;
}

static uint32_t read_int24(mov_ctx_t *ctx)
{
    uint32_t v;
    v = read_int16(ctx) << 8;
    v = read_int8(ctx) | v;
    return v;
}

static uint32_t read_int32(mov_ctx_t *ctx)
{
    uint32_t v1;
    uint32_t v2;
    v1 = read_int16(ctx);
    v2 = read_int16(ctx);
    return (v1 << 16) | v2;
}

static uint64_t read_int48(mov_ctx_t *ctx)
{
    uint64_t v;
    v = (uint64_t)read_int16(ctx) << 32;
    v |= read_int32(ctx);
    return v;
}

static uint64_t read_int64(mov_ctx_t *ctx)
{
    uint32_t v1;
    uint32_t v2;
    v1 = read_int32(ctx);
    v2 = read_int32(ctx);
    return ((uint64_t)v1 << 32) | v2;
}

static int skip_bytes(mov_ctx_t *ctx, int64_t bytes)
{
    if (bytes == 0) {
        return 0;
    }

    int ret = _fseeki64(ctx->f, bytes, SEEK_CUR);
    if (ret != 0) {
        printf("seek failed. skip bytes: %lld\n", bytes);
    }
    return ret;
}

static int read_bytes(mov_ctx_t *ctx, int64_t bytes, void *dst)
{
    if (bytes == 0) {
        return 0;
    }

    int ret = (int)fread(dst, bytes, 1, ctx->f);
    if (ret != 1) {
        printf("read bytes failed. ret: %d, bytes: %lld\n", ret, bytes);
    }
    return 0;
}
