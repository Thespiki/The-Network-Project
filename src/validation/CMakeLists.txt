# Validation: static checks over a project. Reads the model, never changes it,
# and knows nothing about the simulator or the UI.
tnp_add_module(tnp_validation
    SOURCES
        ValidationIssue.h
        ValidationIssue.cpp
        ValidationRule.h
        ValidationRule.cpp
        BuiltinRules.h
        BuiltinRules.cpp
        NetworkValidator.h
        NetworkValidator.cpp
    PUBLIC_DEPS
        tnp::tnp_core)
