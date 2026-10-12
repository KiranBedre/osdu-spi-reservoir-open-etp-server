################################################################################
# Helper macros for simplify find/usage third-party libraries
################################################################################

# Enable Open MP in code.
macro( USE_OPENMP )
    # Allow OpenMP usage:
    find_package(OpenMP REQUIRED)
    if (OPENMP_FOUND)
        if( NOT "${CMAKE_C_FLAGS}" MATCHES "${OpenMP_C_FLAGS}" )
            set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        endif()
        if( NOT "${CMAKE_CXX_FLAGS}" MATCHES "${OpenMP_CXX_FLAGS}" )
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        endif()

        # Uniformize 3rd parties variables
        set(OPENMP_LIBRARIES "${OpenMP_CXX_LIBRARIES}")
    endif()
endmacro()


# Macro for using AvroCPP inside a binary
macro( USE_AVROCPP )
    find_package(unofficial-avro-cpp CONFIG REQUIRED)
    set(AVROCPP_LIBRARIES "unofficial::avro-cpp::avrocpp")
endmacro()


# Macro for using BOOST inside a binary
macro( USE_BOOST )
    # Disable Boost warning "New Boost version may have incorrect or missing dependencies and imported targets" (no issues with dependencies now).
    # Warning can be fixed by upgrading CMake to 3.24 (requirements of Boost 1.80), but this version of CMake can be unavailable on some platforms/IDEs.
    set(Boost_NO_WARN_NEW_VERSIONS 1)

    set(BOOST_MODULES iostreams program_options regex system unit_test_framework)
    # Boost::filesystem and Boost::random are not used directly in the Open-ETP-server, but required by other components (linkage error on Windows if remove)
    if( WIN32 )
       list(APPEND BOOST_MODULES filesystem random)
    endif()
    find_package(Boost REQUIRED
        COMPONENTS
            ${BOOST_MODULES}
    )

    # Add include library for headers resolving
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

    # Uniformize 3rd parties variables
    set(BOOST_LIBRARIES ${Boost_LIBRARIES})
endmacro()


# Macro for using cURL inside a binary
macro( USE_CURL )
    find_package(CURL CONFIG REQUIRED)

    # Add include library for headers resolving
    include_directories(SYSTEM "${CURL_INCLUDE_DIRS}" )
endmacro()


# Macro for using HDF5 inside a binary
macro( USE_HDF5 )
    find_package(HDF5
        COMPONENTS CXX C HL
        REQUIRED
    )

    # Add include library for headers resolving
    include_directories(SYSTEM "${HDF5_INCLUDE_DIRS}" )
endmacro()


macro( USE_NLOHMANN )
    find_package(nlohmann_json CONFIG REQUIRED)
    set(NLOHMANN_LIBRARIES "nlohmann_json::nlohmann_json")
endmacro()


# Macro for using OpenSSL inside a binary
macro( USE_OPENSSL )
    find_package(OpenSSL REQUIRED)

    # Add include library for headers resolving
    include_directories(SYSTEM "${OPENSSL_INCLUDE_DIR}" )
endmacro()


# Macro for using PostgreSQL inside a binary
macro( USE_POSTGRESQL )
    find_package(PostgreSQL REQUIRED)

    # Uniformize 3rd parties variables
    set(POSTGRESQL_LIBRARIES "${PostgreSQL_LIBRARIES}")
endmacro()


macro( USE_WEBSOCKETPP )
    find_package(websocketpp CONFIG REQUIRED)
    set(WEBSOCKETPP_LIBRARIES "websocketpp::websocketpp")
endmacro()


macro( USE_PROMETHEUS )
    find_package(prometheus-cpp CONFIG REQUIRED)
    set(PROMETHEUS_LIBRARIES "prometheus-cpp::pull")
endmacro()


macro( USE_CPPJWT )
    find_package(cpp-jwt CONFIG REQUIRED)
    set(CPPJWT_LIBRARIES "cpp-jwt::cpp-jwt")
endmacro()


macro( USE_FMT )
    find_package(fmt CONFIG REQUIRED)
    set(FMT_LIBRARIES "fmt::fmt")
endmacro()


macro( USE_MINIZIP )
    # This uses the unofficial minizip lib which is a fork of the original code.
    # The original is deprecated and no longer maintained.
    find_package(unofficial-minizip CONFIG REQUIRED)
    set(MINIZIP_LIBRARIES "unofficial::minizip::minizip")
endmacro()


macro( USE_READERWRITERQUEUE )
    find_path(READERWRITERQUEUE_INCLUDE_DIRS "readerwriterqueue/atomicops.h")
    # Usage:
      # target_include_directories(main PRIVATE ${READERWRITERQUEUE_INCLUDE_DIRS})
endmacro()

macro( USE_GOOGLECLOUD )
    find_package(google_cloud_cpp_rest_internal CONFIG REQUIRED)
    set(GC_LIBRARIES "google-cloud-cpp::rest_internal")
endmacro()

macro( USE_PROTOBUF )
    find_package(Protobuf CONFIG REQUIRED)
    set(PROTOBUF_LIBRARIES "protobuf::libprotobuf")
endmacro()

macro( USE_OPENTELEMETRY )
    find_package(opentelemetry-cpp CONFIG REQUIRED)
    # Add include library for headers resolving
    include_directories(SYSTEM "${OPENTELEMETRY_CPP_INCLUDE_DIRS}" )
    endmacro()

macro( USE_AWS )
    find_package(AWSSDK REQUIRED COMPONENTS secretsmanager ssm)
    find_package(ZLIB)

    set(AWS_LIBRARIES ${AWSSDK_LINK_LIBRARIES} ZLIB::ZLIB)
endmacro()

macro( USE_AZURE )
    find_package(azure-identity-cpp CONFIG REQUIRED)
    find_package(azure-core-cpp CONFIG REQUIRED)
    set(AZURE_LIBRARIES "Azure::azure-identity" "Azure::azure-core")
endmacro()

macro( USE_TINYXML2 )
    find_package(tinyxml2 CONFIG REQUIRED)
    set(TINYXML2_LIBRARIES "tinyxml2::tinyxml2")
endmacro()