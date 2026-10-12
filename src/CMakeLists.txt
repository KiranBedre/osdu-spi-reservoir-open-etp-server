
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_subdirectory(lib)
add_subdirectory(bin)

if ( BUILD_TESTS )
    add_subdirectory(tests)
endif()

if ( BUILD_BENCHMARK )
    add_subdirectory(benchmark)
endif()
