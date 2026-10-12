project(oes_http)
add_compile_definitions(oes_core_http_EXPORTS)

# 3rd parties:
    USE_CURL()

# files needed for build
    include(files.cmake)

add_library(
    oes_http SHARED

    ${oes_http_HEADERS}
    ${oes_http_SOURCES}
)
target_link_libraries(
    oes_http
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE

    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${CURL_LIBRARIES}
)
