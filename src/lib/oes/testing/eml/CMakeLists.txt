oes_use_boost(unit_test_framework filesystem date_time)
oes_use_pthread(RPATH_LINK)

set(TARGET oes_testing_eml)

oes_add(INCLUDEPATH ${OES_PROJECT_DIR}/src/lib)

oes_add(SOURCES
    ConsoleUtils.cpp
    OpenKVClientServerFixture.cpp
    OsduOpenKVClientServerFixture.cpp
    ServerFixture.cpp
    TestingMocks.cpp
)
oes_add(HEADERS
    export.h
    ConsoleUtils.h
    OpenKVClientServerFixture.h
    OsduOpenKVClientServerFixture.h
    ServerFixture.h
    ServerTesting.hpp
    TestingMocks.h
)

oes_add(LIBS
    oes_testing_core
    oes_core_utils
    oes_eml
)
oes_add_library()

### Installation rules
oes_install_target()
oes_install_headers(include/oes/testing/eml)
oes_install_sources(src/lib/oes/testing/eml)
