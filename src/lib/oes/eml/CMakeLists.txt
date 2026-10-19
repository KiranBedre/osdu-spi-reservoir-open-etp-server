project(oes_eml)
add_compile_definitions(oes_eml_EXPORTS)

# 3rd parties:
    USE_AVROCPP()
    USE_BOOST()
    USE_WEBSOCKETPP()
    USE_PROMETHEUS()
    USE_FMT()
    USE_POSTGRESQL()
    USE_CURL()
    USE_PROTOBUF()
    USE_NLOHMANN()
    USE_OPENTELEMETRY()

# files needed for build
    include(files.cmake)


# Special options for compilation:
    if(MSVC)
        set_source_files_properties(
            etp12/Server.cpp
            PROPERTIES COMPILE_FLAGS /bigobj
        )
        # C files in EPC are part of the MiniZip project. Lower down the warning level. 
        set_source_files_properties(
            epc/crypt.h
            epc/ioapi.c
            epc/unzip.c
            epc/zip.c
            PROPERTIES COMPILE_FLAGS /W0
        )
    endif()

    # Add last Git commit information definition
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 "--pretty=%H"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _build_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 "--pretty=%ad"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _build_timestamp
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        message( STATUS "GIT hash: ${_build_version}")
        message( STATUS "GIT time: ${_build_timestamp}")
        string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")
        set_property(
            SOURCE etp12/Server.cpp
            APPEND
            PROPERTY COMPILE_DEFINITIONS
            GIT_COMMIT_HASH=${_build_version} GIT_COMMIT_TIMESTAMP=${_build_timestamp} BUILD_TIME=${BUILD_TIME}
        )
    endif()

add_library(
    oes_eml SHARED

    ${oes_eml_HEADERS}
    ${oes_eml_SOURCES}
    ${oes_eml_RESOURCE_HEADERS}
)

target_include_directories(oes_eml PRIVATE ${READERWRITERQUEUE_INCLUDE_DIRS})

target_link_libraries(
    oes_eml
    # Internal library that are explicitelly inside header files
    PUBLIC
        oes_osdu
        oes_utils
        oes_postgresql
        oes_http
        oes_client
        oes_common
        oes_providers
    # Internal library that are only in source files
    PRIVATE

    # 3rd Parties:
    PRIVATE
        ${PROMETHEUS_LIBRARIES}
        ${AVROCPP_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${WEBSOCKETPP_LIBRARIES}
        ${FMT_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
        ${CPPJWT_LIBRARIES}
        ${FMT_LIBRARIES}
        ${MINIZIP_LIBRARIES}
        ${CURL_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${OPENTELEMETRY_CPP_LIBRARIES}
)