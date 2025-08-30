#pragma once

#include <stdint.h>

struct amf0_t;

typedef enum amf0_types_t
{
    AMF0_NUMBER = 0,
    AMF0_BOOL,
    AMF0_STRING,
    AMF0_OBJECT,
    AMF0_MOVIE_CLIP,

    AMF0_NULL = 5,
    AMF0_UNDEFINED = 6,
    AMF0_REFERENCE = 7,
    AMF0_ECMAARRAY = 8,
    AMF0_OBJ_END = 9,
    AMF0_STRICT_ARRAY = 10,
    AMF0_DATE = 11,
    AMF0_LONG_STRING = 12
} amf0_types_t;

typedef struct amf0_property_t
{
    char *name;
    struct amf0_t *value;
} amf0_property_t;

typedef struct amf0_t
{
    uint8_t version;
    amf0_types_t type;

    union
    {
        double num;
        int boolean;
        char *str;
    } d;

    // for object.
    size_t obj_property_count;
    amf0_property_t *obj_properties;

    // for ecma array
    uint32_t ecma_array_count;
    amf0_property_t *ecma_properties;
} amf0_t;


// @param[out] v
// @return If error, return negative values. If success, return the bytes used
//     for parsing.
int amf0_parse(uint8_t *data, size_t len, amf0_t *v);

// Convert amf0_t instance to a display message, for use in debug or log.
void amf0_to_string(amf0_t *v, char *msg, size_t msg_len);

const char *amf0_type_to_string(amf0_types_t type);

/*************** amf0 writing ***************/
/**
 * All writing functions return negative values on error.
 * If success, bytes written is returned.
 */
int amf0_write_number(double num, uint8_t *dst, size_t dst_len);
int amf0_write_bool(uint8_t boolean, uint8_t *dst, size_t dst_len);
int amf0_write_string(const char *str, uint8_t *dst, size_t dst_len);
int amf0_write_null(uint8_t *dst, size_t dst_len);


// object writing flow:
// 1. write obj start.
// 2. write property name, then write the value which may be number or string or others.
// 3. write all properties like in step 2.
// 4. write obj end.
int amf0_write_obj_start(uint8_t *dst, size_t dst_len);
int amf0_write_obj_property_name(const char *property_name, uint8_t *dst, size_t dst_len);
int amf0_write_obj_end(uint8_t *dst, size_t dst_len);

