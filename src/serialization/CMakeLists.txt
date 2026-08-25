# Serialization: the .tnpjson document format and the .tnp container.
#
# nlohmann/json is linked PRIVATE on purpose: no other module should acquire a
# dependency on the JSON library through this one.
tnp_add_module(tnp_serialization
    SOURCES
        JsonSerializer.h
        JsonSerializer.cpp
        ProjectSerializer.h
        ProjectSerializer.cpp
        TnpContainer.h
        TnpContainer.cpp
        TnpCrypto.h
        TnpCrypto.cpp
    PUBLIC_DEPS
        tnp::tnp_core
    PRIVATE_DEPS
        nlohmann_json::nlohmann_json)
