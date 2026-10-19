project(oes_osdu)
add_compile_definitions(oes_osdu_EXPORTS)

# 3rd parties:
    USE_CURL()
    USE_NLOHMANN()
    USE_FMT()
    USE_PROTOBUF()
    USE_OPENTELEMETRY()

# files needed for build
include(files.cmake)

add_library(
        oes_osdu SHARED
        ${oes_osdu_HEADERS}
        ${oes_osdu_SOURCES}
)

target_link_libraries(
        oes_osdu
        # Internal library that are explicitelly inside header files
        PUBLIC

        # Internal library that are only in source files
        PRIVATE
        oes_http
        oes_utils
        oes_providers
        oes_postgresql

        # 3rd Parties:
        PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${CURL_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${FMT_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        ${OPENTELEMETRY_CPP_LIBRARIES}
)
