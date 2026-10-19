project(run_test_common_auth)

# 3rd parties:
    USE_BOOST()
    USE_FMT()
    USE_NLOHMANN()


# files needed for build
    include(files.cmake)

add_executable(
    run_test_common_auth
    ${run_test_common_auth_HEADERS}
    ${run_test_common_auth_SOURCES}
)

# add googlemock 
find_package(GTest CONFIG REQUIRED)
#--

target_link_libraries(
    run_test_common_auth
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        oes_utils
        oes_testing
        oes_http # DBG temporary for entitlement http tests
        oes_common
    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${BOOST_LIBRARIES}
        GTest::gmock
        ${FMT_LIBRARIES}
)

add_test(
    NAME run_test_common_auth
    COMMAND run_test_common_auth
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)