project(run_test_common_epc)

# 3rd parties:
    USE_FMT()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_common_epc
    ${run_test_common_epc_HEADERS}
    ${run_test_common_epc_SOURCES}
)

target_link_libraries(
    run_test_common_epc
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
        ${BOOST_LIBRARIES}
        ${FMT_LIBRARIES}
)

add_test(
    NAME run_test_common_epc
    COMMAND run_test_common_epc
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(
    run_test_common_epc PROPERTIES LABELS "UNIT"
)