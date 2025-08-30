#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flv_parse_functions.h"
#include "flv_defs.h"

int main(int argc, char *argv[])
{
    int ret;
    printf("start flv_main.\n");

    if (argc < 2) {
        fprintf(stdout, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    printf("Processing file: %s\n", filename);

    flv_ctx_t *ctx = malloc(sizeof(flv_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    ret = parse_flv_file(filename, ctx);
    if (ret != 0) {
        printf("Failed to parse flv file\n");
    }

    printf("end flv_main.\n");
    return 0;
}