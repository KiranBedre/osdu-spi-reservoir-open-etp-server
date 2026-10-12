project(run_test_common)

# 3rd parties:
    USE_AVROCPP()
    USE_BOOST()
    USE_FMT()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_common
    ${run_test_common_HEADERS}
    ${run_test_common_SOURCES}
)

target_link_libraries(
    run_test_common
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_testing
        oes_common
    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${AVROCPP_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${FMT_LIBRARIES}
)

add_test(
    NAME run_test_common
    COMMAND run_test_common
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(
    run_test_common PROPERTIES
    ENVIRONMENT "RDMS_DATA_PARTITION_MODE=single;RDMS_DATA_CONNECTIVITY_MODE=standalone"
    LABELS "UNIT"
)

add_subdirectory(auth)
add_subdirectory(epc)
