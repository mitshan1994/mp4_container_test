#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mkv_parse_functions.h"

int main(int argc, char *argv[])
{
    int ret;
    printf("start mkv_test_main.\n");

    if (argc < 2) {
        fprintf(stdout, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    printf("Processing file: %s\n", filename);

    mkv_ctx_t *mkv_ctx = malloc(sizeof(*mkv_ctx));
    memset(mkv_ctx, 0, sizeof(*mkv_ctx));

    ret = parse_mkv_file(filename, mkv_ctx);
    if (ret != 0) {
        printf("[ERROR] FAILED to parse_mkv_file\n");
    } else {
        printf("succeeded parsing\n");
    }
    printf("\n");

    // Print basic info.
    printf("File:\n");
    printf("  timestamp scale: %llu\n", mkv_ctx->ts_scale);
    printf("  segment data start: %llu\n", mkv_ctx->segment_start);
    printf("\n");

    // Print clusters info.
    
    printf("Clusters:\n");
    for (int i = 0; i != mkv_ctx->cluster_count; ++i) {
        printf(" index: %d, timestamp: %llu, real ts: %.2lf. abs content pos:%llu\n", 
            i, 
            mkv_ctx->clusters[i].timestamp,
            1.0 * mkv_ctx->clusters[i].timestamp * mkv_ctx->ts_scale / 1000000000.0,
            mkv_ctx->clusters[i].content_abs_pos);
    }
    printf("\n");
    

    printf("end mkv_test_main.\n");
    return 0;
}