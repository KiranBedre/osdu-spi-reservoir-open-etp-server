project(oes_client)
add_compile_definitions(oes_client_EXPORTS)

# 3rd parties:
    USE_AVROCPP()
    USE_BOOST()
    USE_HDF5()
    USE_OPENSSL()
    USE_WEBSOCKETPP()
    USE_FMT()


# files needed for build
    include(files.cmake)


# Special options for compilation:
    if(MSVC)
        set_source_files_properties(
            Client.cpp
            PROPERTIES COMPILE_FLAGS /bigobj
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
            SOURCE Client.cpp
            APPEND
            PROPERTY COMPILE_DEFINITIONS
            GIT_COMMIT_HASH=${_build_version} GIT_COMMIT_TIMESTAMP=${_build_timestamp} BUILD_TIME=${BUILD_TIME}
        )
    endif()

add_library(
    oes_client SHARED

    ${oes_client_HEADERS}
    ${oes_client_SOURCES}
)

target_link_libraries(
    oes_client
    # Internal library that are explicitelly inside header files
    PUBLIC
        oes_osdu
        oes_utils
        oes_postgresql
        oes_http
        oes_common
    # Internal library that are only in source files
    PRIVATE

    # 3rd Parties:
    PRIVATE
        ${AVROCPP_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${HDF5_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${WEBSOCKETPP_LIBRARIES}
        ${FMT_LIBRARIES}
)