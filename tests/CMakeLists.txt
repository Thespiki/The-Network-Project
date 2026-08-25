# The test suite.
#
# One executable per layer so a failure points straight at the module that broke,
# and so a slow integration run does not sit between a developer and the unit
# tests.

include(Catch)

function(tnp_add_test_executable name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARG_DEPS})
    target_include_directories(${name} PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(${name} PROPERTIES FOLDER "Tests")
    tnp_set_target_options(${name})

    catch_discover_tests(${name})
endfunction()

tnp_add_test_executable(tnp_tests_core
    SOURCES
        core/test_addresses.cpp
        core/test_subnets.cpp
        core/test_interfaces.cpp
        core/test_network.cpp
    DEPS tnp::tnp_core)

tnp_add_test_executable(tnp_tests_protocols
    SOURCES
        protocols/test_ethernet_arp.cpp
        protocols/test_ipv4_icmp.cpp
        protocols/test_transport.cpp
        protocols/test_services.cpp
    DEPS tnp::tnp_core)

tnp_add_test_executable(tnp_tests_routing
    SOURCES
        routing/test_routing_table.cpp
        routing/test_static_routing.cpp
    DEPS tnp::tnp_core)

tnp_add_test_executable(tnp_tests_simulation
    SOURCES
        simulation/test_event_queue.cpp
        simulation/test_arp_and_switching.cpp
        simulation/test_forwarding.cpp
        simulation/test_packet_decoder.cpp
    DEPS tnp::tnp_simulation)

tnp_add_test_executable(tnp_tests_serialization
    SOURCES
        serialization/test_roundtrip.cpp
        serialization/test_container.cpp
        serialization/test_malformed.cpp
    DEPS tnp::tnp_app)

tnp_add_test_executable(tnp_tests_validation
    SOURCES
        validation/test_rules.cpp
    DEPS tnp::tnp_app)

tnp_add_test_executable(tnp_tests_integration
    SOURCES
        integration/test_commands.cpp
        integration/test_cli.cpp
        integration/test_workflow.cpp
    DEPS tnp::tnp_app)
