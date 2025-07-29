cmake_minimum_required (VERSION 3.8)

project ("mp4_container_tests")

set(CMAKE_CXX_STANDARD 17)

include_directories(
	$ENV{BOOST74_INC_DIR}
)
link_directories(
	$ENV{BOOST74_LIB_DIR}
)

# Include sub-projects.
add_subdirectory ("src")
