add_executable (mp4_data_extract 
	"mp4_format/mp4_data_extract.c"
	"mp4_format/mov_defs.h"
	"mp4_format/mov_read_functions.c"
	"read_utils.h"
	"read_utils.c"
	"decoder_config_record.h"
	"decoder_config_record.c"
)

add_executable (mpeg_ts_parse
	"mpeg2_format/mpeg_parse_functions.c"
	"mpeg2_format/mpeg_test_main.c"
	"mpeg2_format/mpeg_parse_functions.h"
	"mpeg2_format/mpeg_defs.h"
	"read_utils.h"
	"read_utils.c"
)

add_executable (mkv_parse
	"mkv_format/mkv_test_main.c"
	"mkv_format/mkv_parse_functions.h"
	"mkv_format/mkv_parse_functions.c"
	"mkv_format/mkv_defs.h"
	"mkv_format/mkv_type_map.h"
	"read_utils.h"
	"read_utils.c"
	"decoder_config_record.h"
	"decoder_config_record.c"
	"mkv_format/mkv_element_handlers.h"
	"mkv_format/mkv_element_handlers.c"
	"mkv_format/mkv_internal_func.h"
)

add_executable (flv_parse
	"read_utils.h"
	"read_utils.c"
	"decoder_config_record.h"
	"decoder_config_record.c"
	"AMF.h"
	"AMF.c"

	"flv_format/flv_main.c"
	"flv_format/flv_defs.h"
	"flv_format/flv_parse_functions.h"
	"flv_format/flv_parse_functions.c"
)

add_executable (rtmp_client_test
	"read_utils.h"
	"read_utils.c"
	"AMF.h"
	"AMF.c"

	"rtmp_format/rtmp_test_main.c"
	"rtmp_format/rtmp_defs.h"
	"rtmp_format/rtmp_parse_functions.h"
	"rtmp_format/rtmp_parse_functions.c"
)
target_link_libraries(rtmp_client_test
	ws2_32.lib
)

include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

add_compile_definitions(
	_CRT_SECURE_NO_WARNINGS
)
