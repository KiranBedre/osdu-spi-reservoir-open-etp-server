project(run_test_testing)

# 3rd parties:
    USE_BOOST()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_testing
    ${run_test_testing_HEADERS}
    ${run_test_testing_SOURCES}
)

target_link_libraries(
    run_test_testing
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_testing
    # 3rd Parties:
    PRIVATE
        ${BOOST_LIBRARIES}
)

add_test(
    NAME run_test_testing
    COMMAND run_test_testing
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
