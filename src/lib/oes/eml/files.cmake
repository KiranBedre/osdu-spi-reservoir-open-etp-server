# List all files required for eml library

################################################################################
# root directory
set( oes_eml_SOURCES
    OpenETPServerConfig.cpp
)
SOURCE_GROUP( "" FILES ${oes_eml_SOURCES} )
set( oes_eml_HEADERS
    export.h
    fwd.h
    OpenETPServerConfig.h
)
SOURCE_GROUP( "" FILES ${oes_eml_HEADERS} )

################################################################################
# etp12 directory
set( oes_eml_etp12_SOURCES
    etp12/Extensions.cpp
    etp12/MetricsConf.cpp
    etp12/MsgUtils.cpp
    etp12/Server.cpp
)
LIST(APPEND oes_eml_SOURCES ${oes_eml_etp12_SOURCES})
SOURCE_GROUP( "etp12" FILES ${oes_eml_etp12_SOURCES} )
set( oes_eml_etp12_HEADERS
    etp12/Extensions.h
    etp12/MetricsConf.h
    etp12/MsgUtils.h
    etp12/MultiPartReply.hpp
    etp12/Server.h
    etp12/Server.hpp
)
LIST(APPEND oes_eml_HEADERS ${oes_eml_etp12_HEADERS})
SOURCE_GROUP( "etp12" FILES ${oes_eml_etp12_HEADERS} )


################################################################################
# openkv directory
set( oes_eml_openkv_SOURCES
    openkv/AdminDB.cpp
    openkv/AuditLog.cpp
    openkv/BaseProto.cpp
    openkv/ArrayProto.cpp
    openkv/DiscoProto.cpp
    openkv/PGStream.cpp
    openkv/Repo.cpp
    openkv/ServerConfig.cpp
    openkv/Session.cpp
    openkv/SpaceDDL.cpp
    openkv/SpaceDML.cpp
    openkv/SpaceProto.cpp
    openkv/SpaceOSDUProto.cpp
    openkv/StoreProto.cpp
    openkv/StoreOSDUProto.cpp
    openkv/TransProto.cpp
    openkv/TypesProto.cpp
)
LIST(APPEND oes_eml_SOURCES ${oes_eml_openkv_SOURCES})
SOURCE_GROUP( "openkv" FILES ${oes_eml_openkv_SOURCES} )
set( oes_eml_openkv_HEADERS
    openkv/AdminDB.h
    openkv/AuditLog.h
    openkv/BaseProto.h
    openkv/ArrayProto.h
    openkv/DiscoProto.h
    openkv/PGStream.h
    openkv/Repo.h
    openkv/ServerConfig.h
    openkv/Session.h
    openkv/SpaceDDL.h
    openkv/SpaceDML.h
    openkv/SpaceOSDUProto.h
    openkv/SpaceProto.h
    openkv/StoreOSDUProto.h
    openkv/StoreProto.h
    openkv/TransProto.h
    openkv/TypesProto.h
)
LIST(APPEND oes_eml_HEADERS ${oes_eml_openkv_HEADERS})
SOURCE_GROUP( "openkv" FILES ${oes_eml_openkv_HEADERS} )
