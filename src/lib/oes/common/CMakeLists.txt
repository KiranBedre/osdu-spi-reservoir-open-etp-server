project(oes_common)
add_compile_definitions(oes_common_EXPORTS)

# 3rd parties:
    USE_AVROCPP()
    USE_BOOST()
    USE_NLOHMANN()
    USE_WEBSOCKETPP()
    USE_CPPJWT()
    USE_FMT()
    USE_MINIZIP()
    USE_READERWRITERQUEUE()
    USE_TINYXML2()


# files needed for build
    include(files.cmake)

# Add ETP Specs file as embedded resource:
    # TODO: This is too complicated and should be replaced by a simple copy
    #       in the build tree and reading the file at runtime.
    include(${HELPER_LIBRARY_DIR}/embed_resource.cmake)
    embed_resource("etp12/etp-1.2.avpr" "etp-1.2-avpr.h" "etp_1_2_avpr")
    set(oes_common_RESOURCE_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/etp-1.2-avpr.h")
    include_directories(${CMAKE_CURRENT_BINARY_DIR})


# Special options for compilation:
    if(MSVC)
        set_source_files_properties(
            etp12/Messages.cpp
            etp12/Protocols.cpp
            PROPERTIES COMPILE_FLAGS /bigobj
        )
    else()
        # TODO: with other optimization options we get crash at exit 100% time:
        set_source_files_properties(EtpSchemas.cpp PROPERTIES COMPILE_FLAGS -O0)
    endif()

add_library(
    oes_common SHARED

    ${oes_common_HEADERS}
    ${oes_common_SOURCES}
    ${oes_common_RESOURCE_HEADERS}
)

target_include_directories(oes_common PRIVATE ${READERWRITERQUEUE_INCLUDE_DIRS})

target_link_libraries(
    oes_common
    # Internal library that are explicitelly inside header files
    PUBLIC
        oes_osdu
        oes_utils
        oes_postgresql
        oes_http
    # Internal library that are only in source files
    PRIVATE

    # 3rd Parties:
    PRIVATE
        ${AVROCPP_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
        ${CPPJWT_LIBRARIES}
        ${FMT_LIBRARIES}
        ${MINIZIP_LIBRARIES}
        ${TINYXML2_LIBRARIES}
)