#/////////////////////////

# List all files required for common library
################################################################################
# root directory
set( oes_common_SOURCES
    AvroBinary.cpp
    AvroUtils.cpp
    CompressUtils.c
    EtpSchemas.cpp
    HdfAry.cpp
)
SOURCE_GROUP( "" FILES ${oes_common_SOURCES} )
set( oes_common_HEADERS
    fwd.h
    AvroBinary.h
    AvroUtils.h
    CompressUtils.h
    CompressUtils.hpp
    EtpSchemas.h
    export.h
    QueueUtils.hpp
    HdfAry.h
)
SOURCE_GROUP( "" FILES ${oes_common_HEADERS} )


################################################################################
# auth directory
set( oes_common_auth_SOURCES
    auth/AuthN.cpp
    auth/DelegateClient.cpp
    auth/DelegateClientException.cpp
    auth/Entitlement.cpp
    auth/JWToken.cpp
)
LIST(APPEND oes_common_SOURCES ${oes_common_auth_SOURCES})
SOURCE_GROUP( "auth" FILES ${oes_common_auth_SOURCES} )
set( oes_common_auth_HEADERS
    auth/AuthN.h
    auth/DelegateClient.h
    auth/DelegateClientException.h
    auth/Entitlement.h
    auth/JWToken.h
)
LIST(APPEND oes_common_HEADERS ${oes_common_auth_HEADERS})
SOURCE_GROUP( "auth" FILES ${oes_common_auth_HEADERS} )

################################################################################
# epc directory
set( oes_common_epc_SOURCES
    epc/ActivityUtils.cpp
    epc/ContentType.cpp
    epc/FilePart.cpp
    epc/FileRelationship.cpp
    epc/Relationship.cpp
    epc/CoreProperty.cpp
    epc/Package.cpp
    epc/FileContentType.cpp
    epc/FileCoreProperties.cpp
    epc/XmlUtils.cpp
)
LIST(APPEND oes_common_SOURCES ${oes_common_epc_SOURCES})
SOURCE_GROUP( "epc" FILES ${oes_common_epc_SOURCES} )
set( oes_common_epc_HEADERS
    epc/ActivityUtils.h
    epc/ContentType.h
    epc/FilePart.h
    epc/FileRelationship.h
    epc/Relationship.h
    epc/CoreProperty.h
    epc/Package.h
    epc/FileContentType.h
    epc/FileCoreProperties.h
    epc/XmlUtils.h
    epc/rapidxml_ns.hpp
)
LIST(APPEND oes_common_HEADERS ${oes_common_epc_HEADERS})
SOURCE_GROUP( "epc" FILES ${oes_common_epc_HEADERS} )

################################################################################
# etp12 directory
set( oes_common_etp12_SOURCES
    etp12/EmlUri.cpp
    etp12/Messages.cpp
    etp12/Metadata.cpp
    etp12/Protocols.cpp
    etp12/Tracer.cpp
    etp12/ArrayUtils.cpp
)
LIST(APPEND oes_common_SOURCES ${oes_common_etp12_SOURCES})
SOURCE_GROUP( "etp12" FILES ${oes_common_etp12_SOURCES} )
set( oes_common_etp12_HEADERS
    etp12/ArrayUtils.h
    etp12/EmlUri.h
    etp12/EncoderSender.hpp
    etp12/ErrorCodes.h
    etp12/Messages.h
    etp12/Messages.hpp
    etp12/Metadata.h
    etp12/MsgFlags.h
    etp12/Protocols.h
    etp12/Tracer.h
)
LIST(APPEND oes_common_HEADERS ${oes_common_etp12_HEADERS})
SOURCE_GROUP( "etp12" FILES ${oes_common_etp12_HEADERS} )
