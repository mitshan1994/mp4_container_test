#pragma once

#include <stdint.h>
#include <stdio.h>

uint32_t read_int8(FILE *f);
uint32_t read_int16(FILE *f);
uint32_t read_int24(FILE *f);
uint32_t read_int32(FILE *f);
uint64_t read_int48(FILE *f);
uint64_t read_int64(FILE *f);

// Skip given bytes in reading context.
// @return 0 on success.
int skip_bytes(FILE *f, int64_t bytes);
int read_bytes(FILE *f, int64_t bytes, void *dst);

// Parse from buffer.
inline uint8_t get_int8(uint8_t *data) { return data[0]; }
inline uint16_t get_int16(uint8_t *data) { return (data[0] << 8) + data[1]; }
inline uint32_t get_int24(uint8_t *data) { return (data[0] << 16) + (data[1] << 8) + data[2]; }
inline uint32_t get_int32(uint8_t *data) { return (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]; }
inline uint64_t get_int64(uint8_t *data) { return (((uint64_t)get_int32(data)) << 32) + get_int32(data + 4); }

inline void set_int8(uint8_t *dst, uint8_t v) { *dst = v; }
inline void set_int16(uint8_t *dst, uint16_t v) { *dst = v >> 8; *(dst + 1) = v & 0xFF; }
inline void set_int24(uint8_t *dst, uint32_t v) { set_int8(dst, v >> 16); set_int16(dst + 1, v & 0xFFFF); }
inline void set_int32(uint8_t *dst, uint32_t v) { set_int16(dst, v >> 16); set_int16(dst + 2, v & 0xFFFF); }
inline void set_int64(uint8_t *dst, uint64_t v) { set_int32(dst, v >> 32); set_int32(dst + 4, v); }
