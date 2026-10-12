project(oes_postgresql)
add_compile_definitions(oes_postgresql_EXPORTS)

# 3rd parties:
    USE_BOOST()
    USE_POSTGRESQL()
    USE_FMT()
    USE_CURL()
    USE_PROTOBUF()
    USE_NLOHMANN()
    USE_OPENTELEMETRY()


# files needed for build
    include(files.cmake)

add_library(
    oes_postgresql SHARED

    ${oes_postgresql_HEADERS}
    ${oes_postgresql_SOURCES}
)

target_link_libraries(
    oes_postgresql
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils 

    # 3rd Parties:
    PRIVATE
        ${BOOST_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
        ${FMT_LIBRARIES}
        ${CURL_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${OPENTELEMETRY_CPP_LIBRARIES}
)
