# cmake_minimum_required() also set the newly-introduced policies to NEW
# See: https://cmake.org/cmake/help/latest/manual/cmake-policies.7.html
cmake_minimum_required(VERSION 3.21)

if( POLICY CMP0074 )
    # New policy for detecting packages libraries was introduced in CMake 3.12
    # See https://cmake.org/cmake/help/v3.12/policy/CMP0074.html
    cmake_policy(SET CMP0074 "NEW")
endif()

if( POLICY CMP0077 )
    # New policy for option() behavior is to honor already existing cache entry
    # See https://cmake.org/cmake/help/v3.13/policy/CMP0077.html
    cmake_policy(SET CMP0077 "NEW")
endif()

# Organize projects into folders
# See: http://athile.net/library/blog/?p=288
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Enable usages of VCPKG manifest file 'vcpkg.json'
set(VCPKG_FEATURE_FLAGS "versions")

# Main Project for the whole repo
project( OpenETPServer )


################################################################################
# Options for tuning build rules

# Choose ABI policy
# see https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html
option( OLD_CXX11_ABI "Force old ABI policy for glibcxx" )

option(CODE_COVERAGE "Enable code coverage")
message(STATUS "[Option] CODE_COVERAGE: ${CODE_COVERAGE}")

option(BUILD_OSDU_STUB_SERVER "Build OSDU server stub" ON)
message(STATUS "[Option] BUILD_OSDU_STUB_SERVER: ${BUILD_OSDU_STUB_SERVER}")

option(BUILD_BENCHMARK "Build benchmark" ON)
message(STATUS "[Option] BUILD_BENCHMARK: ${BUILD_BENCHMARK}")

option(BUILD_TESTS "Build tests" ON)
message(STATUS "[Option] BUILD_TESTS: ${BUILD_TESTS}")

################################################################################
# Define C/CXX standards

# Defines the C++ Standard to use (for GCC, equivalent to -std=c++17)
set( CMAKE_CXX_STANDARD 17 )

# Defines the C Standard to use (for GCC, equivalent to -std=c90 or -ansi)
set( CMAKE_C_STANDARD 90 )

set( CMAKE_CXX_STANDARD_REQUIRED ON )
# CMake uses extended variants of language dialects by default, such as -std=gnu++14 instead of -std=c++14
# Disable that.
set( CMAKE_CXX_EXTENSIONS OFF )


################################################################################
# Select the output directries for binaries

# Generate all binaries at the root of the build tree:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(      RUNTIME_OUTPUT_PATH      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# put libraries in a dedicated "lib" directory
if( WIN32 )
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(      LIBRARY_OUTPUT_PATH      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(      ARCHIVE_OUTPUT_PATH      ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
else()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(      LIBRARY_OUTPUT_PATH      ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
endif()


set( HELPER_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE INTERNAL "Where to find CMake helper scripts." FORCE )
# Base definitions for 3rd parties:
include(${HELPER_LIBRARY_DIR}/3rd_parties.cmake)
include(${HELPER_LIBRARY_DIR}/source_group_by_folder.cmake)

################################################################################
# OS/Compiler options:
if( UNIX )
    # Required for fix RPATH in the installed binaries (see also: /src/bin/openETPServer/CMakeLists.txt)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_definitions(
            "-fdiagnostics-color=always" # To use color in messages with Ninja
            "-pipe"                # Use pipes instead of creating temporary files (improves performance)
            "-Wall" "-Wextra"      # Output warnings
        )
        add_definitions( -fPIC )

        if( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7" AND OLD_CXX11_ABI)
            add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")
        endif()

        # Ensure GOLD linker is used
        if( NOT CMAKE_SHARED_LINKER_FLAGS MATCHES "-fuse-ld=gold")
            set( CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold" )
            set( CMAKE_EXE_LINKER_FLAGS        "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold" )
        endif()

        # Do not allow unresolved symbols for neither library nor executable.
        if( NOT CMAKE_SHARED_LINKER_FLAGS MATCHES "--no-undefined")
            set( CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
            set( CMAKE_EXE_LINKER_FLAGS        "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined" )
        endif()

        # Ease GDB's life for faster debugging
        if( NOT CMAKE_SHARED_LINKER_FLAGS MATCHES "--gdb-index")
            set( CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gdb-index" )
            set( CMAKE_EXE_LINKER_FLAGS        "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index" )
        endif()

        # enable code covering
        if( CODE_COVERAGE )
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        endif()
    endif()
else()
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0601)

    # Faster builds by excluding some of the less common API declarations such as Cryptography, DDE, RPC, Shell and Windows Sockets
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
    set(VCPKG_OVERLAY_TRIPLETS "triplets")
    add_compile_options(/W4 /D_CRT_SECURE_NO_WARNINGS /utf-8)
    # Fixes VS build error: c101008d: Failed to write the updated manifest to the resource of file "oes_utils.dll"
    add_link_options(/MANIFEST:NO)
endif ()

################################################################################
# Enable testing via CTest for the project
enable_testing()


################################################################################
# Custom definitions for project
add_definitions(
    -DOES_EML_AUTH_NO_CRYPTO
    # By commenting next line lots of "not supported in this build" issues in UT were resolved:
    #-DOES_COMMON_ETP12_NO_AVRO_JSON
)

IF(CMAKE_BUILD_TYPE MATCHES "Debug")
    ADD_DEFINITIONS(-DOES_DEBUG_BUILD)
ENDIF()

add_subdirectory(src)
