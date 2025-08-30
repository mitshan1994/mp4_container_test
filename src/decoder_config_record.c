
#include "decoder_config_record.h"

#include <stdlib.h>
#include <string.h>

int avc_decoder_record_parse(uint8_t *data, size_t data_len, uint8_t **pp_sps, size_t *out_sps_len, 
    uint8_t **pp_pps, size_t *out_pps_len)
{
    const uint8_t *start_pos = data;

    uint8_t config_version = *data++;
    uint8_t profile_indication = *data++;
    uint8_t profile_compat = *data++;
    uint8_t level_indication = *data++;

    uint8_t b = *data++;
    if ((b >> 2) != 0x3F) {
        printf("  AVCDecoderConfigurationRecord bits not '111111' before lengthSizeMinusOne\n");
    }
    uint8_t length_size = (b & 0x3) + 1;

    b = *data++;
    if ((b >> 5) != 0x7) {
        printf("  AVCDecoderConfigurationRecord bits not '111' before numOfSequenceParameterSets\n");
    }
    uint8_t num_sps = (b & 0x1F);
    for (int i = 0; i != num_sps; ++i) {
        uint16_t sps_len = get_int16(data);
        data += 2;
        // Only read first sps.
        if (i == 0) {
            *pp_sps = malloc(sps_len);
            memcpy(*pp_sps, data, sps_len);
            *out_sps_len = sps_len;
            data += sps_len;
        } else {
            data += sps_len;
        }
    }

    uint8_t num_pps = *data++;
    for (int i = 0; i != num_pps; ++i) {
        uint16_t pps_len = get_int16(data);
        data += 2;
        // Only read first pps.
        if (i == 0) {
            *pp_pps = malloc(pps_len);
            memcpy(*pp_pps, data, pps_len);
            *out_pps_len = pps_len;
            data += pps_len;
        } else {
            data += pps_len;
        }
    }

    return 0;
}
