project(run_test_postgresql)

# 3rd parties:
    USE_BOOST()
    USE_POSTGRESQL()
    USE_FMT()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_postgresql
    ${run_test_postgresql_HEADERS}
    ${run_test_postgresql_SOURCES}
)

target_link_libraries(
    run_test_postgresql
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_testing
        oes_postgresql

    # 3rd Parties:
    PRIVATE
        ${BOOST_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
        ${FMT_LIBRARIES}
)

add_test(
   NAME run_test_postgresql
   COMMAND run_test_postgresql
   WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(
    run_test_postgresql PROPERTIES LABELS "REQUIRES_POSTGRESQL"
)

