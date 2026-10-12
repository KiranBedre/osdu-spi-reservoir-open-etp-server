project(run_test_osdu_eml)

# 3rd parties:
USE_BOOST()
USE_FMT()

# files needed for build
include(files.cmake)

add_executable(
    run_test_osdu_eml
    ${run_test_osdu_eml_HEADERS}
    ${run_test_osdu_eml_SOURCES}
)

target_link_libraries(
    run_test_osdu_eml
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_eml
        oes_testing
    # 3rd Parties:
    PRIVATE
        ${BOOST_LIBRARIES}
        ${FMT_LIBRARIES}
)

add_test(
    NAME run_test_osdu_eml
    COMMAND run_test_osdu_eml
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(
    run_test_osdu_eml PROPERTIES
    ENVIRONMENT "RDMS_DATA_PARTITION_MODE=multiple;RDMS_DATA_CONNECTIVITY_MODE=osdu"
    LABELS "REQUIRES_POSTGRESQL"
)
