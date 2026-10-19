# List all files required for openETPServer executable

################################################################################
# root directory
set( openETPServer_SOURCES
    main.cpp
    EmlAppContext.cpp
    EtpClient12.cpp
    Hdf5Cmds.cpp
    ProbeCmds.cpp
    ServerCmds.cpp
    SpaceCmds.cpp
)
SOURCE_GROUP( "" FILES ${openETPServer_SOURCES} )
set( openETPServer_HEADERS
    EmlAppContext.h
    EtpClient12.h
)
SOURCE_GROUP( "" FILES ${openETPServer_HEADERS} )
