# Application layer: document lifecycle, preferences, autosave and recovery,
# selection, learning mode and export. Knows nothing about rendering.
tnp_add_module(tnp_app
    SOURCES
        ApplicationConfig.h
        ApplicationConfig.cpp
        ProjectFile.h
        ProjectFile.cpp
        Selection.h
        Selection.cpp
        LearningNarrator.h
        LearningNarrator.cpp
        TopologyExport.h
        TopologyExport.cpp
        SampleProject.h
        SampleProject.cpp
        Application.h
        Application.cpp
    PUBLIC_DEPS
        tnp::tnp_cli
        tnp::tnp_commands
        tnp::tnp_serialization
        tnp::tnp_simulation
        tnp::tnp_testing
        tnp::tnp_validation
    PRIVATE_DEPS
        nlohmann_json::nlohmann_json)

target_compile_definitions(tnp_app PUBLIC TNP_VERSION_STRING="${PROJECT_VERSION}")
