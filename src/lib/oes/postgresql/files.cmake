# List all files required for postgresql library

################################################################################
# core directory
set( oes_postgresql_SOURCES
    Connection.cpp
    DbConnectionPool.cpp
    ErrorCodes.cpp
    PhysicalName.cpp
    Statement.cpp
    Transaction.cpp
)
set( oes_postgresql_HEADERS
    Connection.h
    DbConnectionPool.h
    ErrorCodes.h
    Format.hpp
    PhysicalName.h
    Statement.h
    Transaction.h
    export.h
    fwd.h
)
