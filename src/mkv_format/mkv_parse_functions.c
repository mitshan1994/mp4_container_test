#include <string.h>
#include <stdlib.h>

#include "mkv_parse_functions.h"
#include "read_utils.h"
#include "mkv_type_map.h"
#include "mkv_internal_func.h"

// Accelerated macros for reading ints
#define read8()    read_int8_mkv(ctx)
#define read16()   read_int16_mkv(ctx)
#define read24()   read_int24_mkv(ctx)
#define read32()   read_int32_mkv(ctx)
#define read48()   read_int48_mkv(ctx)
#define read64()   read_int64_mkv(ctx)

static int parse_next_element(mkv_ctx_t *ctx);

element_type_t get_type_by_id(uint64_t id)
{
    int i = 0;
    while (type_map[i].type != ELE_UNKNOWN) {
        if (type_map[i].id == id) {
            return type_map[i].type;
        }
        ++i;
    }
    return ELE_UNKNOWN;
}

const char *get_desc_by_id(uint64_t id)
{
    int i = 0;
    while (type_map[i].type != ELE_UNKNOWN) {
        if (type_map[i].id == id) {
            return type_map[i].desc;
        }
        ++i;
    }
    return type_map[i].desc;
}

ele_handler_func_t get_element_handler_by_id(uint64_t id)
{
    int i = 0;
    while (type_map[i].type != ELE_UNKNOWN) {
        if (type_map[i].id == id) {
            break;
        }
        ++i;
    }
    return type_map[i].ele_handler;
}

const char *get_data_type_str(element_type_t type)
{
    switch (type) {
    case ELE_MASTER:
        return "MASTER";
    case ELE_BINARY:
        return "BINARY";
    case ELE_UTF8:
        return "UTF8";
    case ELE_ASCII:
        return "ASCII";
    case ELE_UINT:
        return "UINT";
    case ELE_SINT:
        return "SINT";
    case ELE_FLOAT:
        return "FLOAT";
    case ELE_DATE:
        return "DATE";
    default:
        return "UNKNOWN";
    }
}

const char *get_depth_space(uint32_t depth)
{
    switch (depth) {
    case 0:
        return "";
    case 1:
        return "  ";
    case 2:
        return "    ";
    case 3:
        return "      ";
    case 4:
        return "        ";
    case 5:
        return "          ";
    default:
        return "            ";
    }
}

mkv_track_t *mkv_get_track_by_id(mkv_ctx_t *ctx, uint64_t trackid)
{
    for (int i = 0; i != ctx->track_count; ++i) {
        if (ctx->tracks[i]->id == trackid) {
            return ctx->tracks[i];
        }
    }
    return NULL;
}

// @param[out] v Parsed VINT value
// @param[out] used_len If not NULL, set to the bytes of the VINT.
int parse_VINT(mkv_ctx_t *ctx, uint64_t *v, uint8_t *used_len)
{
    int ret;
    uint8_t b;
    uint8_t bytes[8];
    uint8_t len;
    b = read8();

    if (b & 0x80) {
        len = 1;
    } else if (b & 0x40) {
        len = 2;
    } else if (b & 0x20) {
        len = 3;
    } else if (b & 0x10) {
        len = 4;
    } else if (b & 0x08) {
        len = 5;
    } else if (b & 0x04) {
        len = 6;
    } else if (b & 0x02) {
        len = 7;
    } else if (b & 0x01) {
        len = 8;
    } else {
        printf("invalid element ID first byte: %x\n", b);
        return -1;
    }

    bytes[0] = b;
    // mask first "len bits".
    bytes[0] = (uint8_t)(bytes[0] << len) >> len;
    ret = read_bytes_mkv(ctx, len - 1, bytes + 1);

    uint64_t value = 0;
    for (size_t i = 0; i != len; ++i) {
        value = (value << 8) + bytes[i];
    }

    *v = value;
    if (used_len) {
        *used_len = len;
    }
    return 0;
}

int get_VINT(uint8_t *buf, uint64_t buf_len, uint64_t *v, uint8_t *used_len)
{
    if (buf_len == 0) {
        return -1;
    }

    int ret;
    uint8_t b;
    uint8_t bytes[8];
    uint8_t len;
    b = buf[0];

    if (b & 0x80) {
        len = 1;
    } else if (b & 0x40) {
        len = 2;
    } else if (b & 0x20) {
        len = 3;
    } else if (b & 0x10) {
        len = 4;
    } else if (b & 0x08) {
        len = 5;
    } else if (b & 0x04) {
        len = 6;
    } else if (b & 0x02) {
        len = 7;
    } else if (b & 0x01) {
        len = 8;
    } else {
        printf("invalid element ID first byte: %x\n", b);
        return -1;
    }

    if (buf_len < len) {
        printf("buffer size too short. cannot parse VINT from it.\n");
        return -1;
    }

    bytes[0] = b;
    // mask first "len bits".
    bytes[0] = (uint8_t)(bytes[0] << len) >> len;
    memcpy(bytes + 1, buf + 1, len - 1);

    uint64_t value = 0;
    for (size_t i = 0; i != len; ++i) {
        value = (value << 8) + bytes[i];
    }

    *v = value;
    if (used_len) {
        *used_len = len;
    }
    return 0;
}

static int parse_element_size_type(mkv_ctx_t *ctx, mkv_element_t *element)
{
    int ret;
    uint8_t used_len;

    ret = parse_VINT(ctx, &element->id, &used_len);
    if (ret != 0) {
        printf("failed to parse Element ID!\n");
        return ret;
    }
    // Update ID with the prefix 0s and 1.
    element->id |= ((uint64_t)0x1) << (8 * (used_len - 1)) << (8 - used_len);

    ret = parse_VINT(ctx, &element->data_size, NULL);
    if (ret != 0) {
        printf("failed to parse Element Data Size!\n");
        return ret;
    }

    element->type = get_type_by_id(element->id);
    element->desc = get_desc_by_id(element->id);

    return 0;
}

static int parse_master_element(mkv_ctx_t *ctx, mkv_element_t element, 
    ele_handler_func_t handler)
{
    int ret = 0;
    uint64_t start_pos = _ftelli64(ctx->f);
    uint64_t end_pos = start_pos + element.data_size;

    if (handler) {
        ret = handler(ctx, NULL, 0);
        if (ret != 0) {
            printf("master handler failed.\n");
            return -1;
        }
    }

    while (_ftelli64(ctx->f) < end_pos) {
        ret = parse_next_element(ctx);
        if (ret != 0) {
            printf("failed to parse element in Master Element.\n");
            break;
        }
    }

    if (_ftelli64(ctx->f) != end_pos) {
        printf("end pos remain: %d. adjust forcely\n", (int)(end_pos - _ftelli64(ctx->f)));
        _fseeki64(ctx->f, end_pos, SEEK_SET);
    }

    return ret;
}

static int parse_uint_element(mkv_ctx_t *ctx, mkv_element_t element,
    ele_handler_func_t handler)
{
    uint64_t v;
    uint64_t data_size = element.data_size;
    uint8_t buf[8] = { 0 };

    if (data_size == 0) {
        v = 0;
    } else if (data_size > 8) {
        printf("invalid UINT data size: %llu\n", data_size);
        return -1;
    } else {
        for (size_t i = 0; i != data_size; ++i) {
            buf[data_size - i - 1] = read8();
        }

        v = *(uint64_t *)buf;
    }
    printf("%sUINT value: %llu\n", get_depth_space(ctx->depth), v);

    if (handler) {
        return handler(ctx, &v, sizeof(v));
    }

    return 0;
}

static int parse_float_element(mkv_ctx_t *ctx, mkv_element_t element,
    ele_handler_func_t handler)
{
    double v;
    uint64_t data_size = element.data_size;

    if (0 == data_size) {
        v = 0;
    } else if (4 == data_size) {
        uint32_t i32 = read32();
        v = *(float *)&i32;
    } else if (8 == data_size) {
        uint64_t i64 = read64();
        v = *(double *)&i64;
    } else {
        printf("invalid FLOAT data size: %llu\n", data_size);
        return -1;
    }

    printf("%sFLOAT value: %lf\n", get_depth_space(ctx->depth), v);

    if (handler) {
        return handler(ctx, &v, sizeof(v));
    }
    return 0;
}

static int parse_ascii_element(mkv_ctx_t *ctx, mkv_element_t element,
    ele_handler_func_t handler)
{
    int ret;
    uint64_t data_size = element.data_size;

    if (data_size == 0) {
        printf("empty ASCII string\n");
        return handler ? handler(ctx, "", 0) : 0;
    }

    char *data = malloc(data_size + 1);
    data[data_size] = '\0';

    ret = read_bytes_mkv(ctx, data_size, data);
    if (ret != 0) {
        printf("failed to read bytes: %llu\n", data_size);
        return ret;
    }
    printf("%sASCII value: %s\n", get_depth_space(ctx->depth), data);

    if (handler) {
        ret = handler(ctx, data, data_size);
    }

    free(data);

    return ret;
}

static int parse_utf8_element(mkv_ctx_t *ctx, mkv_element_t element,
    ele_handler_func_t handler)
{
    int ret;
    uint64_t data_size = element.data_size;

    if (data_size == 0) {
        printf("empty UTF8 string\n");
        return handler ? handler(ctx, "", 0) : 0;
    }

    char *data = malloc(data_size + 1);
    data[data_size] = '\0';

    ret = read_bytes_mkv(ctx, data_size, data);
    if (ret != 0) {
        printf("failed to read bytes: %llu\n", data_size);
        return ret;
    }
    // TODO Convert UTF8 to gbk before print.
    printf("%sUTF8 value: %s\n", get_depth_space(ctx->depth), data);

    if (handler) {
        ret = handler(ctx, data, data_size);
    }

    free(data);

    return ret;
}

static int parse_binary_element(mkv_ctx_t *ctx, mkv_element_t element,
    ele_handler_func_t handler)
{
    int ret;
    uint64_t data_size = element.data_size;

    if (data_size == 0) {
        printf("empty binary\n");
        return handler ? handler(ctx, "", 0) : 0;
    }

    uint8_t *data = malloc(data_size + 1);
    data[data_size] = '\0';

    ret = read_bytes_mkv(ctx, data_size, data);
    if (ret != 0) {
        printf("failed to read bytes: %llu\n", data_size);
        free(data);
        return ret;
    }

    if (handler) {
        ret = handler(ctx, data, data_size);
    }
    free(data);

    return ret;
}

static int parse_next_element(mkv_ctx_t *ctx)
{
    int ret;

    mkv_element_t element;
    memset(&element, 0, sizeof(element));
    ret = parse_element_size_type(ctx, &element);
    if (ret != 0) {
        printf("failed to parse element id and size!\n");
        return -1;
    }

    printf("%selement id: 0x%llX (%s) %s, data size: %llu\n", 
        get_depth_space(ctx->depth), 
        element.id, 
        element.desc ? element.desc : "",
        get_data_type_str(element.type),
        element.data_size
    );

    ele_handler_func_t ele_handler = get_element_handler_by_id(element.id);

    uint64_t start_pos = _ftelli64(ctx->f);
    uint64_t end_pos = start_pos + element.data_size;

    ctx->depth++;

    if (element.type == ELE_MASTER) {
        ret = parse_master_element(ctx, element, ele_handler);
    } else if (element.type == ELE_UINT) {
        ret = parse_uint_element(ctx, element, ele_handler);
    } else if (element.type == ELE_FLOAT) {
        ret = parse_float_element(ctx, element, ele_handler);
    } else if (element.type == ELE_ASCII) {
        ret = parse_ascii_element(ctx, element, ele_handler);
    } else if (element.type == ELE_UTF8) {
        ret = parse_utf8_element(ctx, element, ele_handler);
    } else if (element.type == ELE_BINARY) {
        ret = parse_binary_element(ctx, element, ele_handler);
    } else {
        ret = skip_bytes_mkv(ctx, element.data_size);
    }

    if (_ftelli64(ctx->f) != end_pos) {
        printf("end pos remain: %d. adjust forcely\n", (int)(end_pos - _ftelli64(ctx->f)));
        _fseeki64(ctx->f, end_pos, SEEK_SET);
    }

    ctx->depth--;

    return ret;
}

int parse_mkv_file(const char *filename, mkv_ctx_t *ctx)
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
    printf("file size: %llu\n", file_size);

    for (;;) {
        if (_ftelli64(ctx->f) >= file_size - 4) {
            printf("reaching file end\n");
            break;
        }

        ret = parse_next_element(ctx);
        if (ret != 0) {
            printf("failed to parse next element\n");
            break;
        }
    }

    return 0;
}
