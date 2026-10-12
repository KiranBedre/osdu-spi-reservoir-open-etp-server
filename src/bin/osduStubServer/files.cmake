# List all files required for osduStubServer executable

################################################################################
# root directory
set( osduStubServer_SOURCES
    main.cpp
    osduStubServerConfig.cpp
)
SOURCE_GROUP( "" FILES ${osduStubServer_SOURCES} )

set( osduStubServer_HEADERS
    osduStubServerConfig.h
)
SOURCE_GROUP( "" FILES ${osduStubServer_HEADERS} )


################################################################################
# http directory
set( osduStubServer_http_SOURCES
    http/HttpContext.h
    http/BaseRequestHandlers.h
    http/HttpServerStub.h
)
LIST(APPEND osduStubServer_SOURCES ${osduStubServer_http_SOURCES})
SOURCE_GROUP( "http" FILES ${osduStubServer_http_SOURCES} )
set( osduStubServer_http_HEADERS
    http/HttpContext.cpp
    http/BaseRequestHandlers.cpp
    http/HttpServerStub.cpp
)
LIST(APPEND osduStubServer_HEADERS ${osduStubServer_http_HEADERS})
SOURCE_GROUP( "http" FILES ${osduStubServer_http_HEADERS} )


################################################################################
# osdu directory
set( osduStubServer_osdu_SOURCES
    osdu/PartitionServiceStub.cpp
    osdu/KeyVaultStub.cpp
    osdu/EntitlementsStub.cpp
    osdu/StorageStub.cpp
)
LIST(APPEND osduStubServer_SOURCES ${osduStubServer_osdu_SOURCES})
SOURCE_GROUP( "osdu" FILES ${osduStubServer_osdu_SOURCES} )
set( osduStubServer_osdu_HEADERS
    osdu/PartitionServiceStub.h
    osdu/KeyVaultStub.h
    osdu/EntitlementsStub.h
    osdu/StorageStub.h
)
LIST(APPEND osduStubServer_HEADERS ${osduStubServer_osdu_HEADERS})
SOURCE_GROUP( "osdu" FILES ${osduStubServer_osdu_HEADERS} )
