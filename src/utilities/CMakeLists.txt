# Foundation layer: identifiers, time, logging, byte streams, file helpers.
# Depends on nothing but the standard library.
tnp_add_module(tnp_utilities
    SOURCES
        Types.h
        Result.h
        Id.h
        Uuid.h
        Uuid.cpp
        Time.h
        Time.cpp
        StringUtilities.h
        StringUtilities.cpp
        ByteStream.h
        ByteStream.cpp
        Logging.h
        Logging.cpp
        FileSystem.h
        FileSystem.cpp)
