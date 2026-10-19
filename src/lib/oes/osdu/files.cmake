# List all files required for osdu library

################################################################################
# root directory
set( oes_osdu_HEADERS
        export.h
        OsduConnectionProvider.h
)
SOURCE_GROUP( "" FILES ${oes_osdu_HEADERS} )

set( oes_osdu_SOURCES
        OsduConnectionProvider.cpp
)
SOURCE_GROUP( "" FILES ${oes_osdu_SOURCES} )

################################################################################
# api directory
set( oes_osdu_api_HEADERS
        OsduConfig.h
        api/OsduClientException.h
        api/BaseClient.h
        api/PartitionClient.h
        api/EntitlementClient.h
        api/EntitlementClientPolicy.h
        api/EntitlementDataspace.h
        api/EntitlementUtils.h
        api/DSPermission.h
        )
LIST(APPEND oes_osdu_HEADERS ${oes_osdu_api_HEADERS})
SOURCE_GROUP( "api" FILES ${oes_osdu_api_HEADERS} )

set( oes_osdu_api_SOURCES
        OsduConfig.cpp
        api/OsduClientException.cpp
        api/BaseClient.cpp
        api/PartitionClient.cpp
        api/EntitlementClient.cpp
        api/EntitlementClientPolicy.cpp
        api/EntitlementDataspace.cpp
        api/EntitlementUtils.cpp
        api/DSPermission.cpp
        )
LIST(APPEND oes_osdu_SOURCES ${oes_osdu_api_SOURCES})
SOURCE_GROUP( "api" FILES ${oes_osdu_api_SOURCES} )
