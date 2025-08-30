#pragma once

#include "read_utils.h"

/**
 * Parsing several decoder configuration records.
 *
 * Memory is currently allocated with "malloc" inside.
 */

// @param pp_sps To store parsed sps.
// @param sps_len sps data length.
// @return 0 on success
int avc_decoder_record_parse(uint8_t *data, size_t data_len, 
    uint8_t **pp_sps, size_t *out_sps_len,
    uint8_t **pp_pps, size_t *out_pps_len);
