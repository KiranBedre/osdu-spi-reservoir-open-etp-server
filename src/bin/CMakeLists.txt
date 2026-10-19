add_subdirectory(openETPServer)

if ( BUILD_OSDU_STUB_SERVER )
    add_subdirectory(osduStubServer)
endif()
