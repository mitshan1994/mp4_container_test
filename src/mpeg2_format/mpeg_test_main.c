#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpeg_defs.h"
#include "mpeg_parse_functions.h"

int main(int argc, char *argv[])
{
    int ret;
    printf("start mpeg_test_main.\n");

    if (argc < 3) {
        fprintf(stdout, "Usage: %s [ts|ps] <filename>\n", argv[0]);
        return 1;
    }

    const char *filetype = argv[1];
    const char *filename = argv[2];
    printf("File type: %s, file: %s\n", filetype, filename);

    if (strncmp(filetype, "ps", 2) != 0 && strncmp(filetype, "ts", 2) != 0) {
        printf("Only accept file type ts or ps\n");
        return 1;
    }
    int is_ts = strncmp(filetype, "ts", 2) == 0;

    mpeg_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ret = parse_mpeg_file(filename, &ctx, is_ts);
    if (ret != 0) {
        printf("parse mpeg failed!\n");
        return 1;
    }
    printf("parse mpeg OK\n");

    return 0;
}