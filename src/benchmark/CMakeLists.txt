project(oesBenchmark)

# 3rd parties:
USE_BOOST()
USE_FMT()
# ToDo: Should not be required (hide details of PostgreSql lib from public interface of 'oes_postgresql')
USE_POSTGRESQL()
find_package(benchmark CONFIG REQUIRED)

# Project sources
file (GLOB_RECURSE oesBenchmark_HEADERS  RELATIVE ${CMAKE_CURRENT_LIST_DIR}  "*.h")
file (GLOB_RECURSE oesBenchmark_SOURCES  RELATIVE ${CMAKE_CURRENT_LIST_DIR}  "*.cpp")
SOURCE_GROUP_BY_FOLDER(${PROJECT_NAME})

# Target settings
add_executable(${PROJECT_NAME}
    ${oesBenchmark_SOURCES} ${oesBenchmark_HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        oes_utils
        oes_eml
        oes_postgresql
        oes_testing
        oes_common
        oes_client
        
    # 3rd Parties:
    PRIVATE
        ${BOOST_LIBRARIES}
        ${FMT_LIBRARIES}
        # ToDo: Should not be required (hide details of PostgreSql lib from public interface of 'oes_postgresql')
        ${POSTGRESQL_LIBRARIES}
        benchmark::benchmark benchmark::benchmark_main
)

file(COPY "${CMAKE_SOURCE_DIR}/config/oesBenchmark_config.json"
     DESTINATION "${CMAKE_BINARY_DIR}")