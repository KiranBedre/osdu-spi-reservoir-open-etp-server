project(oes_providers)
add_compile_definitions(oes_providers_EXPORTS)

# 3rd parties:
USE_GOOGLECLOUD()
USE_CURL()
USE_NLOHMANN()
USE_FMT()
USE_AZURE()

# files needed for build
include(files.cmake)

add_library(
    oes_providers SHARED
    ${oes_providers_HEADERS}
    ${oes_providers_SOURCES}
)

target_link_libraries(
    oes_providers
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
    oes_http
    oes_utils
    oes_postgresql

    # 3rd Parties:
    PRIVATE
    $<$<PLATFORM_ID:Linux>:pthread>
    ${GC_LIBRARIES}
    ${CURL_LIBRARIES}
    ${NLOHMANN_LIBRARIES}
    ${FMT_LIBRARIES}
    ${AZURE_LIBRARIES}
)
