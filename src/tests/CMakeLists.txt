
# All Unit Tests binaries must be generated in a tests directory.
# Except Windows platform, where is no good way to list and copy DLLs and PDB files after build (they don't exist on CMake generate step).
if( NOT WIN32 )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests)
    set(      RUNTIME_OUTPUT_PATH      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

add_subdirectory(data)
add_subdirectory(core)
add_subdirectory(testing)
add_subdirectory(eml)
add_subdirectory(executables)
add_subdirectory(osdu_eml)
add_subdirectory(postgresql)
add_subdirectory(client)
add_subdirectory(common)

file(COPY "${CMAKE_SOURCE_DIR}/config/tests_config.json"
     DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/config/tests_config_local.json"
     DESTINATION "${CMAKE_BINARY_DIR}")