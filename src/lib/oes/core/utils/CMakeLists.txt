project(oes_utils)
add_compile_definitions(oes_core_utils_EXPORTS)

# 3rd parties:
    USE_BOOST()
    USE_PROMETHEUS()
    USE_FMT()
    USE_CURL()
    USE_PROTOBUF()
    USE_NLOHMANN()
    USE_OPENTELEMETRY()

# files needed for build
    include(files.cmake)

add_library(
    oes_utils SHARED

    ${oes_utils_HEADERS}
    ${oes_utils_SOURCES}
)

target_link_libraries(
    oes_utils
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE

    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Windows>:Ws2_32.lib>
        ${PROMETHEUS_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${FMT_LIBRARIES}
        ${CURL_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${OPENTELEMETRY_CPP_LIBRARIES}

)
