project(openETPServer)

# 3rd parties:
    USE_AVROCPP()
    USE_BOOST()
    USE_HDF5()
    USE_POSTGRESQL()
    USE_WEBSOCKETPP()
    USE_FMT()
    USE_CURL()
    USE_PROTOBUF()
    USE_NLOHMANN()
    USE_OPENTELEMETRY()


# files needed for build
    include(files.cmake)

add_executable(
    openETPServer
    ${openETPServer_HEADERS}
    ${openETPServer_SOURCES}
)

set( dependency_libs 
    oes_utils oes_http oes_providers
    oes_osdu oes_eml oes_postgresql
    oes_client oes_common
)

target_link_libraries(
    openETPServer
    # Internal library that are explicitelly inside header files
    PUBLIC

    # Internal library that are only in source files
    PRIVATE
        ${dependency_libs}
        
    # 3rd Parties:
    PRIVATE
        ${AVROCPP_LIBRARIES}
        # ToDo: link only Boost components which actaully used (currently links all including for example unit_test_framework). Review all other places with Boost linkage.
        ${BOOST_LIBRARIES}
        ${HDF5_LIBRARIES}
        ${WEBSOCKETPP_LIBRARIES}
        ${POSTGRESQL_LIBRARIES}
        ${FMT_LIBRARIES}
        ${CURL_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${OPENTELEMETRY_CPP_LIBRARIES}
)

file(COPY "${CMAKE_SOURCE_DIR}/config/openETPServer_config.json"
     DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/config/openETPServer_config_local.json"
     DESTINATION "${CMAKE_BINARY_DIR}")

# Install Open-ETP-server
include(GNUInstallDirs)
# Set path to third-party libraries
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
)

foreach( _lib ${dependency_libs} )
    set_target_properties( ${_lib}
        PROPERTIES
            INSTALL_RPATH "$ORIGIN"
    )
endforeach()

install(
    FILES "${CMAKE_SOURCE_DIR}/config/openETPServer_config.json"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(
    TARGETS ${PROJECT_NAME}
    ${dependency_libs}
    RUNTIME_DEPENDENCY_SET runtime_deps
    )
    
# Install third-party libraries
LIST(APPEND pre_exclude_regexes "api-ms-.*") # Windows API
LIST(APPEND pre_exclude_regexes "ext-ms-.*") # Windows API
LIST(APPEND post_exclude_regexes ".*WINDOWS[\\/]system32.*")    # Exclude Windows system dlls
LIST(APPEND post_exclude_regexes "^/lib" "^/usr" "^/bin")       # Exclude Unix shared libs (all must go from VCPKG)
install(RUNTIME_DEPENDENCY_SET runtime_deps
    PRE_EXCLUDE_REGEXES ${pre_exclude_regexes}
    POST_EXCLUDE_REGEXES ${post_exclude_regexes}
)
