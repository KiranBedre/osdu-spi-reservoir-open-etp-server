add_subdirectory(core)

add_subdirectory(eml)

add_subdirectory(osdu)

add_subdirectory(postgresql)

add_subdirectory(providers)

add_subdirectory(client)

add_subdirectory(common)

if ( BUILD_TESTS OR BUILD_BENCHMARK )
    add_subdirectory(testing)
endif()
