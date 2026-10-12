project(run_test_client)

# 3rd parties:
    USE_BOOST()
    USE_FMT()

# files needed for build
    include(files.cmake)

add_executable(
    run_test_client
    ${run_test_client_HEADERS}
    ${run_test_client_SOURCES}
)

target_link_libraries(
    run_test_client
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_client
        oes_utils
        oes_eml
        oes_testing
        oes_common
    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${BOOST_LIBRARIES}
        ${FMT_LIBRARIES}
)

add_test(
    NAME run_test_client
    COMMAND run_test_client
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(
    run_test_client PROPERTIES
    ENVIRONMENT "RDMS_DATA_PARTITION_MODE=single;RDMS_DATA_CONNECTIVITY_MODE=standalone"
    LABELS "REQUIRES_POSTGRESQL"
)
