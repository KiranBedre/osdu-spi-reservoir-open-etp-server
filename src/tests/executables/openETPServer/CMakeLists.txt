project(run_test_openETPServer)

# 3rd parties:
    USE_AVROCPP()
    USE_BOOST()
    USE_POSTGRESQL()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_openETPServer
    ${run_test_openETPServer_HEADERS}
    ${run_test_openETPServer_SOURCES}
)

target_link_libraries(
    run_test_openETPServer
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_testing
        oes_eml
    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${AVROCPP_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
)

add_test(
    NAME run_test_openETPServer
    COMMAND run_test_openETPServer
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(
    run_test_openETPServer PROPERTIES
    ENVIRONMENT "RDMS_DATA_PARTITION_MODE=single;RDMS_DATA_CONNECTIVITY_MODE=standalone"
    LABELS "REQUIRES_POSTGRESQL"
)
