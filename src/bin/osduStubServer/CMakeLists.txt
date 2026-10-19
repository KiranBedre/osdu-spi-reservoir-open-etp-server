project(osduStubServer)

# 3rd parties:
    USE_BOOST()
    USE_FMT()
    USE_WEBSOCKETPP()
    USE_NLOHMANN()

# files needed for build
    include(files.cmake)

add_executable(${PROJECT_NAME}
    ${osduStubServer_HEADERS}
    ${osduStubServer_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        oes_utils

    # 3rd Parties:
    PRIVATE
        $<$<PLATFORM_ID:Linux>:pthread>
        ${BOOST_LIBRARIES}
        ${WEBSOCKETPP_LIBRARIES}
        ${NLOHMANN_LIBRARIES}
        ${FMT_LIBRARIES}
)

file(COPY "${CMAKE_SOURCE_DIR}/config/osduStubServer_config.json"
     DESTINATION "${CMAKE_BINARY_DIR}")