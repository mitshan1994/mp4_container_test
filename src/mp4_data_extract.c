#include "mov_defs.h"
#include "mov_read_functions.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mov_track_t *get_video_track(mov_ctx_t *ctx);
static mov_track_t *get_audio_track(mov_ctx_t *ctx);
static void extract_raw_h26x_video(mov_ctx_t *ctx, const char *filename);
static void extract_raw_aac_audio(mov_ctx_t *ctx, const char *filename);

static const uint8_t prefix_code[] = { 0x00, 0x00, 0x00, 0x01 };

static uint8_t buffer[4 * 1024 * 1024];

int main(int argc, char *argv[])
{
    int ret;

    if (argc < 2) {
        fprintf(stdout, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    printf("Processing file: %s\n", filename);

    mov_ctx_t *mov_ctx = malloc(sizeof(*mov_ctx));
    memset(mov_ctx, 0, sizeof(*mov_ctx));

    ret = parse_mov_file(filename, mov_ctx);
    if (ret != 0) {
        printf("failed to parse_mov_file\n");
        return 1;
    }
    printf("succeeded parsing\n");


#if 0
    // extract raw video data.
    mov_track_t *video_track = get_video_track(mov_ctx);
    if (video_track) {
        if (strncmp(video_track->codec_format, "avc1", 4) == 0) {
            extract_raw_h26x_video(mov_ctx, "mp4_data_extract.264");
        } else if (strncmp(video_track->codec_format, "hvc1", 4) == 0) {
            extract_raw_h26x_video(mov_ctx, "mp4_data_extract.265");
        }
    }
#endif

#if 0
    // extract raw audio data.
    mov_track_t *audio_track = get_audio_track(mov_ctx);
    if (audio_track) {
        if (strncmp(audio_track->codec_format, "mp4a", 4) == 0) {
            extract_raw_aac_audio(mov_ctx, "mp4_data_extract.aac");
        }
    }
#endif

    // TODO clean mov ctx.

    free(mov_ctx);
    printf("end\n");
    return 0;
}

static mov_track_t *get_video_track(mov_ctx_t *ctx)
{
    for (int i = 0; i != ctx->track_count; ++i) {
        if (ctx->tracks[i].is_video) {
            return &ctx->tracks[i];
        }
    }
    return NULL;
}

static mov_track_t *get_audio_track(mov_ctx_t *ctx)
{
    for (int i = 0; i != ctx->track_count; ++i) {
        if (ctx->tracks[i].is_audio) {
            return &ctx->tracks[i];
        }
    }
    return NULL;
}

static int h26x_process_sample(uint8_t *buffer, uint32_t sample_len, FILE *f)
{
    int ret;
    int pos_in_sample = 0;
    while (pos_in_sample < sample_len - 4) {
        uint8_t *pos = buffer + pos_in_sample;
        uint32_t remain_len = sample_len - pos_in_sample - 4;
        uint32_t prefix_len = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3];
        if (prefix_len > remain_len) {
            printf("invalid prefix_len: %u, remain_len: %u\n", prefix_len, remain_len);
            break;
        }

        // Write nalu.
        ret = fwrite(prefix_code, sizeof(prefix_code), 1, f);
        if (ret != 1) {
            printf("failed to write prefix code.\n");
            return -1;
        }
        ret = fwrite(pos + 4, prefix_len, 1, f);
        if (ret != 1) {
            printf("failed to write nalu body.\n");
            return -1;
        }

        pos_in_sample += prefix_len + 4;
        if (pos_in_sample == sample_len) {
            //printf("current sample parsed successfully.\n");
            break;
        }
    }

    return 0;
}

static int aac_process_sample(uint8_t *buffer, uint32_t sample_len, uint32_t frequency,
    uint32_t channel_count, FILE *f)
{
    int ret;

    static int frequency_array[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000,
        11025, 8000
    };
    static uint32_t frequency_array_len = sizeof(frequency_array) / sizeof(int);

    uint8_t frequency_index = frequency_array_len;
    for (int i = 0; i != frequency_array_len; ++i) {
        if (frequency == frequency_array[i]) {
            frequency_index = i;
            break;
        }
    }
    if (frequency_index == frequency_array_len) {
        printf("audio sample frequency not found: %u\n", frequency);
        return -1;
    }

    uint16_t frame_length = 7 + sample_len;

    // Formulate ADTS header.
    uint8_t adts[7];

    // fixed header.
    // syncword
    adts[0] = 0xFF;
    adts[1] = 0xF0;
    // ID, layer, protection_absent.
    adts[1] |= (0x0 << 3) | (0x00 << 1) | 0x1;
    // profile object type. 1 for AAC LC.
    adts[2] = 0x1 << 6;
    // sampling_frequency_index
    adts[2] |= (frequency_index & 0xF) << 2;
    // private bit.
    adts[2] |= 0 << 1;
    // channel_configuration
    adts[2] |= (channel_count >> 2) & 0x1;
    adts[3] = (channel_count & 0x3) << 6;
    // original_copy, home.
    adts[3] |= (0 << 5) | (0 << 4);

    // variable header.
    // copyright_identification_bit, copyright_identification_start
    adts[3] |= (0 << 3) | (0 << 2);
    // aac_frame_length
    adts[3] |= frame_length >> 11;  // 2 bits
    adts[4] = frame_length >> 3;    // 8 bits
    adts[5] = frame_length << 5;   // 3 bits
    // adts_buffer_fullness, all 1s
    adts[5] |= 0x1F;
    adts[6] = 0x3F << 2;
    // number_of_raw_data_blocks_in_frame
    adts[6] |= 0x0;

    ret = fwrite(adts, 7, 1, f);
    if (ret != 1) {
        printf("failed to write adts header to output file\n");
        return -1;
    }

    ret = fwrite(buffer, sample_len, 1, f);
    if (ret != 1) {
        printf("failed to write sample data to output file\n");
        return -1;
    }
    return 0;
}

static void extract_raw_data(mov_ctx_t *ctx, mov_track_t *cur_track, FILE *f)
{
    int ret;
    int is_avc = strncmp(cur_track->codec_format, "avc1", 4) == 0;
    int is_hevc = strncmp(cur_track->codec_format, "hvc1", 4) == 0;
    int is_aac = strncmp(cur_track->codec_format, "mp4a", 4) == 0;

    if (is_hevc) {
        if (cur_track->vps_len && cur_track->sps_len && cur_track->pps_len) {
            fwrite(prefix_code, sizeof(prefix_code), 1, f);
            fwrite(cur_track->vps, cur_track->vps_len, 1, f);
            fwrite(prefix_code, sizeof(prefix_code), 1, f);
            fwrite(cur_track->sps, cur_track->sps_len, 1, f);
            fwrite(prefix_code, sizeof(prefix_code), 1, f);
            fwrite(cur_track->pps, cur_track->pps_len, 1, f);
        }
    }

    if (is_avc) {
        // SPS and PPS.
        if (cur_track->sps_len && cur_track->pps_len) {
            fwrite(prefix_code, sizeof(prefix_code), 1, f);
            fwrite(cur_track->sps, cur_track->sps_len, 1, f);
            fwrite(prefix_code, sizeof(prefix_code), 1, f);
            fwrite(cur_track->pps, cur_track->pps_len, 1, f);
        } else {
            printf("No sps or pps!\n");
            return;
        }
    }

    // Assume length size prefix is 4 bytes.
    if (is_avc && cur_track->length_size != 4) {
        printf("Need length_size equals 4\n");
        return;
    }

    // Check sample to chunk data.
    if (cur_track->stsc_count == 0) {
        printf("No sample chunk data\n");
        return;
    }

    uint32_t cur_chunk = 0;
    uint32_t cur_sample_index = 0;
    int exit = 0;
    int nalu_count = 0;
    for (int i = 0; !exit; ++i, ++cur_chunk) {
        // Check current chunk is present.
        if (cur_chunk >= cur_track->chunk_offset_count) {
            printf("last chunk was processed\n");
            break;
        }

        // Seek to current chunk.
        uint64_t chunk_start_offset = cur_track->chunk_offsets[cur_chunk];
        ret = fseek(ctx->f, chunk_start_offset, SEEK_SET);
        if (ret == -1) {
            printf("failed to seek to: %llu\n", chunk_start_offset);
        }

        // Get sample per chunk.
        uint32_t cur_sample_per_chunk = 1;
        for (int stsc_idx = 0; stsc_idx != cur_track->stsc_count; ++stsc_idx) {
            if (cur_track->stsc_first_chunk[stsc_idx] <= cur_chunk + 1) {
                cur_sample_per_chunk = cur_track->stsc_sample_per_chunk[stsc_idx];
            } else {
                break;
            }

            if (stsc_idx == cur_track->stsc_count - 1) {
                break;
            }
        }
        //printf("current chunk: %u, sample_per_chunk: %u\n", cur_chunk, cur_sample_per_chunk);

        if (cur_sample_index >= cur_track->sample_lengths_count) {
            printf("cur_sample_index is not less than cur_track->sample_lengths_count!\n");
            exit = 1;
            break;
        }

        // Parse all samples in current chunk, one by one.
        for (int idx_in_chunk = 0; idx_in_chunk < cur_sample_per_chunk; ++idx_in_chunk) {
            // Sample length.
            uint32_t sample_len = cur_track->sample_lengths[cur_sample_index];
            ret = fread(buffer, sample_len, 1, ctx->f);
            if (ret != 1) {
                printf("failed to read %u bytes from file.\n", sample_len);
                exit = 1;
                break;
            }

            if (is_avc || is_hevc) {
                ret = h26x_process_sample(buffer, sample_len, f);
            } else if (is_aac) {
                ret = aac_process_sample(buffer, sample_len, cur_track->audio_sample_rate, 
                    cur_track->channel_count, f);
            }
            
            if (ret != 0) {
                printf("process sample failed\n");
                exit = 1;
                break;
            }

            ++cur_sample_index;
        }

        // Only write the first several nalu.
        //if (nalu_count > 2000) {
        //    printf("current nalu count: %d, stop.\n", nalu_count);
        //    break;
        //}
    }

    printf("nalu count totally processed: %d\n", nalu_count);
}

static void extract_raw_h26x_video(mov_ctx_t *ctx, const char *filename)
{
    printf("\nStart extract raw h26x video to file: %s\n", filename);

    mov_track_t *cur_track = get_video_track(ctx);
    if (NULL == cur_track) {
        printf("No video track found!\n");
        return;
    }
    assert(strncmp(cur_track->codec_format, "avc1", 4) == 0 ||
        strncmp(cur_track->codec_format, "hvc1", 4) == 0);

    FILE *f = fopen(filename, "wb");
    if (NULL == f) {
        printf("failed to open file for writing: %s\n", filename);
        return;
    }

    extract_raw_data(ctx, cur_track, f);

    fclose(f);
    printf("End extract\n");
}

static void extract_raw_aac_audio(mov_ctx_t *ctx, const char *filename)
{
    mov_track_t *cur_track = get_audio_track(ctx);
    if (NULL == cur_track) {
        printf("No video track found!\n");
        return;
    }
    assert(strncmp(cur_track->codec_format, "mp4a", 4) == 0);

    FILE *f = fopen(filename, "wb");
    if (NULL == f) {
        printf("failed to open file for writing: %s\n", filename);
        return;
    }

    extract_raw_data(ctx, cur_track, f);

    fclose(f);
}