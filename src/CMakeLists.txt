add_executable (mp4_data_extract 
	"mp4_data_extract.c"
	"mov_defs.h"
	"mov_read_functions.c"
)

target_compile_definitions(mp4_data_extract PRIVATE
	_CRT_SECURE_NO_WARNINGS
)