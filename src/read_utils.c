#include "read_utils.h"
#include <stdio.h>
#include <stdint.h>

uint32_t read_int8(FILE *f)
{
    uint8_t v;
    size_t ret = fread(&v, sizeof(v), 1, f);
    if (ret != 1) {
        printf("failed to read int8\n");
        return 0;
    }
    return v;
}

uint32_t read_int16(FILE *f)
{
    uint32_t v;
    v = read_int8(f) << 8;
    v = read_int8(f) | v;
    return v;
}

uint32_t read_int24(FILE *f)
{
    uint32_t v;
    v = read_int16(f) << 8;
    v = read_int8(f) | v;
    return v;
}

uint32_t read_int32(FILE *f)
{
    uint32_t v1;
    uint32_t v2;
    v1 = read_int16(f);
    v2 = read_int16(f);
    return (v1 << 16) | v2;
}

uint64_t read_int48(FILE *f)
{
    uint64_t v;
    v = (uint64_t)read_int16(f) << 32;
    v |= read_int32(f);
    return v;
}

uint64_t read_int64(FILE *f)
{
    uint32_t v1;
    uint32_t v2;
    v1 = read_int32(f);
    v2 = read_int32(f);
    return ((uint64_t)v1 << 32) | v2;
}

int skip_bytes(FILE *f, int64_t bytes)
{
    if (bytes == 0) {
        return 0;
    }

    int ret = _fseeki64(f, bytes, SEEK_CUR);
    if (ret != 0) {
        printf("seek failed. skip bytes: %lld\n", bytes);
    }
    return ret;
}

int read_bytes(FILE *f, int64_t bytes, void *dst)
{
    if (bytes == 0) {
        return 0;
    }

    int ret = (int)fread(dst, bytes, 1, f);
    if (ret != 1) {
        printf("read bytes failed. ret: %d, bytes: %lld\n", ret, bytes);
    }
    return 0;
}
