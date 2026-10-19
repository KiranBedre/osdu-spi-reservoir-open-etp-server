set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
# Custom compile flags
if(${PORT} MATCHES "google-cloud-cpp")
    set(VCPKG_CXX_FLAGS "/std:c++20")
    set(VCPKG_C_FLAGS "/std:c++20")
endif()