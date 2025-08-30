#include <stdlib.h>
#include <string.h>

#include "AMF.h"
#include "read_utils.h"

static int amf0_parse_number(uint8_t *data, size_t len, amf0_t *v)
{
    if (len < 8) {
        printf("failed to parse NUMBER. length not enough: %zu\n", len);
        return -1;
    }
    uint64_t i = get_int64(data);
    v->d.num = *(double *)&i;
    return 8;
}

static int amf0_parse_boolean(uint8_t *data, size_t len, amf0_t *v)
{
    if (len < 1) {
        printf("failed to parse BOOLEAN. length not enough: %zu\n", len);
        return -1;
    }
    v->d.boolean = *data;
    return 1;
}

static int amf0_parse_string(uint8_t *data, size_t len, amf0_t *v)
{
    if (len < 2) {
        return -1;
    }

    uint64_t str_len = get_int16(data);
    data += 2;
    len -= 2;

    if (len < str_len) {
        return -1;
    }

    v->d.str = malloc(str_len + 1);
    memcpy(v->d.str, data, str_len);
    v->d.str[str_len] = '\0';
    len -= str_len;

    return str_len + 2;
}

static int amf0_parse_object(uint8_t *data, size_t len, amf0_t *v)
{
    int ret;
    uint8_t *pos = data;
    const uint8_t *pos_end = data + len;

    amf0_property_t *properties[40];    // Should be enough.
    size_t count = 0;

    while (pos < pos_end) {
        // Check obj end.
        if (pos + 3 <= pos_end) {
            if (pos[0] == 0 && pos[1] == 0 && pos[2] == 9) {
                pos += 3;
                break;
            }
        }

        // Property name.
        amf0_t property_name = { 0 };
        property_name.type = AMF0_STRING;
        ret = amf0_parse_string(pos, pos_end - pos, &property_name);
        if (ret < 0) {
            printf("failed to parse property name.\n");
            break;
        }
        pos += ret;

        // PropertyValue
        amf0_t *property_value = malloc(sizeof(*property_value));
        memset(property_value, 0, sizeof(*property_value));
        ret = amf0_parse(pos, pos_end - pos, property_value);
        if (ret < 0) {
            printf("failed to parse object property value.\n");
            free(property_value);
            break;
        }
        pos += ret;

        // Create new property.
        amf0_property_t *property = malloc(sizeof(amf0_property_t));
        property->name = property_name.d.str;
        property->value = property_value;
        properties[count++] = property;
    }

    v->obj_property_count = count;
    if (count > 0) {
        v->obj_properties = malloc(sizeof(amf0_property_t) * count);
        for (int i = 0; i != count; ++i) {
            memcpy(v->obj_properties + i, properties[i], sizeof(amf0_property_t));
            free(properties[i]);
            properties[i] = NULL;
        }
    }

    return pos - data;
}

static int amf0_parse_ecma_array(uint8_t *data, size_t len, amf0_t *v)
{
    int ret;
    const uint8_t *data_start = data;
    if (len < 4) {
        return -1;
    }

    uint32_t associative_count = get_int32(data);
    data += 4;
    len -= 4;

    // Allocate.
    v->ecma_array_count = associative_count;
    v->ecma_properties = malloc(associative_count * sizeof(amf0_property_t));

    for (size_t i = 0; i != associative_count; ++i) {
        amf0_property_t *cur_property = v->ecma_properties + i;

        // PropertyName (SCRIPTDATASTRING)
        amf0_t property_name = { 0 };
        property_name.type = AMF0_STRING;
        ret = amf0_parse_string(data, len, &property_name);
        if (ret < 0) {
            printf("failed to parse property name.\n");
            return ret;
        }
        data += ret;
        len -= ret;
        cur_property->name = property_name.d.str;

        // PropertyValue (SCRIPTDATAVALUE)
        amf0_t *property_value = malloc(sizeof(*property_value));
        memset(property_value, 0, sizeof(*property_value));
        ret = amf0_parse(data, len, property_value);
        if (ret < 0) {
            printf("failed to parse ecma property value.\n");
            return ret;
        }
        data += ret;
        len -= ret;
        cur_property->value = property_value;

        //printf("ecma property name: %s, value type: %d\n", property_name.d.str,
        //    property_value->type);
    }

    // Check SCRIPTDATAOBJECTEND
    if (len < 3) {
        printf("ECMA ARRAY doesn't end with SCRIPTDATA-OBJECTEND\n");
    } else {
        if (data[0] == 0 && data[1] == 0 && data[2] == 9) {
            printf("  (valid SCRIPTDATA-OBJECTEND in ecma array end)\n");
        } else {
            printf("INVALID SCRIPTDATA-OBJECTEND in ecma array end\n");
        }

        len -= 3;
        data += 3;
    }

    return data - data_start;
}

int amf0_parse(uint8_t *data, size_t len, amf0_t *v)
{
    int ret;
    const uint8_t *data_start = data;
    const uint8_t *data_end = data + len;
    uint8_t b;

    if (len == 0 || v == NULL) {
        return -1;
    }

    b = *data++;
    v->type = b;

    ret = -1;
    switch (v->type) {
    case AMF0_NUMBER:
        ret = amf0_parse_number(data, data_end - data, v);
        break;
    case AMF0_BOOL:
        ret = amf0_parse_boolean(data, data_end - data, v);
        break;
    case AMF0_STRING:
        ret = amf0_parse_string(data, data_end - data, v);
        break;
    case AMF0_OBJECT:
        ret = amf0_parse_object(data, data_end - data, v);
        break;
    case AMF0_NULL:
        ret = 0;
        break;
    case AMF0_ECMAARRAY:
        ret = amf0_parse_ecma_array(data, data_end - data, v);
        break;
    default:
        ;
    }

    if (ret < 0) {
        printf("failed to parse amf0 body. type: %d\n", v->type);
        return ret;
    }

    data += ret;

    return data - data_start;
}

void amf0_to_string(amf0_t *v, char *msg, size_t msg_len)
{
    const char *str_type = amf0_type_to_string(v->type);
    char str_value[20];

    switch (v->type) {
    case AMF0_NUMBER:
        snprintf(str_value, sizeof(str_value), "%.3lf", v->d.num);
        break;
    case AMF0_BOOL:
        snprintf(str_value, sizeof(str_value), "%d", v->d.boolean);
        break;
    case AMF0_STRING:
        snprintf(str_value, sizeof(str_value), "%s", v->d.str);
        break;
    case AMF0_OBJECT:
        snprintf(str_value, sizeof(str_value), "%s", "<object>");
        break;
    case AMF0_MOVIE_CLIP:
        break;
    case AMF0_NULL:
        snprintf(str_value, sizeof(str_value), "%s", "<null>");
        break;
    case AMF0_UNDEFINED:
        break;
    case AMF0_REFERENCE:
        snprintf(str_value, sizeof(str_value), "%s", "<reference>");
        break;
    case AMF0_ECMAARRAY:
        snprintf(str_value, sizeof(str_value), "<ecma array of len %u>", v->ecma_array_count);
        break;
    case AMF0_OBJ_END:
        snprintf(str_value, sizeof(str_value), "%s", "<obj end>");
        break;
    case AMF0_STRICT_ARRAY:
        snprintf(str_value, sizeof(str_value), "%s", "<strict array>");
        break;
    case AMF0_DATE:
        snprintf(str_value, sizeof(str_value), "%s", "<date>");
        break;
    case AMF0_LONG_STRING:
        snprintf(str_value, sizeof(str_value), "%s", "<long string>");
        break;
    default:
        break;
    }

    if (strlen(str_value) == 0) {
        snprintf(str_value, sizeof(str_value), "%s", "<???>");
    }

    snprintf(msg, msg_len, "%s(%s)", str_type, str_value);
}

const char *amf0_type_to_string(amf0_types_t type)
{
    switch (type) {
    case AMF0_NUMBER:
        return "NUMBER";
    case AMF0_BOOL:
        return "BOOL";
    case AMF0_STRING:
        return "STRING";
    case AMF0_OBJECT:
        return "OBJECT";
    case AMF0_MOVIE_CLIP:
        return "MOVIE_CLIP";
    case AMF0_NULL:
        return "NULL";
    case AMF0_UNDEFINED:
        return "AMF0_UNDEFINED";
    case AMF0_REFERENCE:
        return "REFERENCE";
    case AMF0_ECMAARRAY:
        return "ECMA-ARRAY";
    case AMF0_OBJ_END:
        return "OBJ_END";
    case AMF0_STRICT_ARRAY:
        return "STRICT_ARRAY";
    case AMF0_DATE:
        return "DATE";
    case AMF0_LONG_STRING:
        return "LONG_STRING";
    default:
        ;
    }

    return "UNKNOWN-AMF0-TYPE";
}

int amf0_write_number(double num, uint8_t *dst, size_t dst_len)
{
    if (dst_len < 9) {
        return -1;
    }

    *dst = AMF0_NUMBER;
    set_int64(dst + 1, *(uint64_t *)&num);

    return 9;
}

int amf0_write_bool(uint8_t boolean, uint8_t *dst, size_t dst_len)
{
    if (dst_len < 2) {
        return -1;
    }

    *dst = AMF0_BOOL;
    *(dst + 1) = boolean;
    return 2;
}

int amf0_write_string(const char *str, uint8_t *dst, size_t dst_len)
{
    size_t str_len = strlen(str);
    if (dst_len < str_len + 3) {
        return -1;
    }

    *dst = AMF0_STRING;
    set_int16(dst + 1, str_len);
    memcpy(dst + 3, str, str_len);

    return str_len + 3;
}

int amf0_write_null(uint8_t *dst, size_t dst_len)
{
    if (dst_len < 1) {
        return -1;
    }
    *dst = AMF0_NULL;

    return 1;
}

int amf0_write_obj_start(uint8_t *dst, size_t dst_len)
{
    if (dst_len < 1) {
        return -1;
    }
    *dst = AMF0_OBJECT;

    return 1;
}

int amf0_write_obj_property_name(const char *property_name, uint8_t *dst, size_t dst_len)
{
    int str_len = strlen(property_name);
    if (dst_len < 2 + str_len) {
        return -1;
    }

    set_int16(dst, str_len);
    memcpy(dst + 2, property_name, str_len);
    return str_len + 2;
}

int amf0_write_obj_end(uint8_t *dst, size_t dst_len)
{
    if (dst_len < 3) {
        return -1;
    }
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 0x9;
    return 3;
}
