project(oes_testing)
# TODO: remove those definitions and use oes_testing_EXPORTS in export.h source code
add_compile_definitions(oes_testing_core_EXPORTS oes_testing_postgresql_EXPORTS oes_testing_eml_EXPORTS)

# 3rd parties:
    USE_BOOST()
    USE_POSTGRESQL()
    USE_FMT()


# files needed for build
    include(files.cmake)

add_library(
    oes_testing SHARED

    ${oes_testing_HEADERS}
    ${oes_testing_SOURCES}
)

# Determine path to OpenETPServer executable
if (${GENERATOR_IS_MULTI_CONFIG})
    set (CMAKE_OPEN_ETP_SERVER_PATH "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/openETPServer")
else()
    set (CMAKE_OPEN_ETP_SERVER_PATH "${CMAKE_BINARY_DIR}/openETPServer")
endif()
target_compile_definitions(oes_testing PUBLIC OPEN_ETP_SERVER_PATH="${CMAKE_OPEN_ETP_SERVER_PATH}")

target_link_libraries(
    oes_testing
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_postgresql
        oes_eml

    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${BOOST_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
        ${FMT_LIBRARIES}
)
