# List all files required for testing library

################################################################################
# core directory
set( oes_testing_core_SOURCES
    core/AssertTestTools.cpp
    core/LoggerTestTools.cpp
    core/ScopedTimer.cpp
    core/TestMain.cpp
    core/TestConfig.cpp
)
LIST(APPEND oes_testing_SOURCES ${oes_testing_core_SOURCES})
SOURCE_GROUP( "core" FILES ${oes_testing_core_SOURCES} )
set( oes_testing_core_HEADERS
    core/AssertTestTools.h
    core/export.h
    core/LoggerTestTools.h
    core/ScopedTimer.h
    core/TestMain.h
    core/TestConfig.h
)
LIST(APPEND oes_testing_HEADERS ${oes_testing_core_HEADERS})
SOURCE_GROUP( "core" FILES ${oes_testing_core_HEADERS} )

################################################################################
# core directory
set( oes_testing_eml_SOURCES
    eml/ConsoleUtils.cpp
    eml/OpenKVClientServerFixture.cpp
    eml/OsduOpenKVClientServerFixture.cpp
    eml/ServerFixture.cpp
    eml/TestingMocks.cpp
)
LIST(APPEND oes_testing_SOURCES ${oes_testing_eml_SOURCES})
SOURCE_GROUP( "eml" FILES ${oes_testing_eml_SOURCES} )
set( oes_testing_eml_HEADERS
    eml/export.h
    eml/ConsoleUtils.h
    eml/OpenKVClientServerFixture.h
    eml/OsduOpenKVClientServerFixture.h
    eml/ServerFixture.h
    eml/ServerTesting.hpp
    eml/TestingMocks.h
)
LIST(APPEND oes_testing_HEADERS ${oes_testing_eml_HEADERS})
SOURCE_GROUP( "eml" FILES ${oes_testing_eml_HEADERS} )


################################################################################
# postgresql directory
set( oes_testing_postgresql_SOURCES
    postgresql/PostgreSQLTestsFixture.cpp
)
LIST(APPEND oes_testing_SOURCES ${oes_testing_postgresql_SOURCES})
SOURCE_GROUP( "postgresql" FILES ${oes_testing_postgresql_SOURCES} )
set( oes_testing_postgresql_HEADERS
    postgresql/export.h
    postgresql/PostgreSQLTestsFixture.h
)
LIST(APPEND oes_testing_HEADERS ${oes_testing_postgresql_HEADERS})
SOURCE_GROUP( "postgresql" FILES ${oes_testing_postgresql_HEADERS} )
