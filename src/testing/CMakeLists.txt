# Automated connectivity tests: runs the assertions stored in a project against
# a private copy of it, driven by the simulator.
tnp_add_module(tnp_testing
    SOURCES
        TestResult.h
        TestResult.cpp
        NetworkTestRunner.h
        NetworkTestRunner.cpp
    PUBLIC_DEPS
        tnp::tnp_simulation
    PRIVATE_DEPS
        tnp::tnp_serialization)
