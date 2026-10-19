project(run_test_utils)

# 3rd parties:
    USE_BOOST()
    USE_OPENMP()
    USE_PROMETHEUS()
    USE_FMT()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_utils
    ${run_test_utils_HEADERS}
    ${run_test_utils_SOURCES}
)

target_link_libraries(
    run_test_utils
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_testing
    # 3rd Parties:
    PRIVATE
        ${PROMETHEUS_LIBRARIES}
        ${BOOST_LIBRARIES}
        ${OPENMP_LIBRARIES}
        ${FMT_LIBRARIES}
)

add_test(
    NAME run_test_utils
    COMMAND run_test_utils
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
