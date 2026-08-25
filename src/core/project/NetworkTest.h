#pragma once

#include "core/network/Ids.h"
#include "core/network/Ipv4Address.h"
#include "utilities/Time.h"

#include <optional>
#include <string>
#include <string_view>

namespace tnp::core {

/// Traffic a test generates.
enum class TestProtocol : u8 { Icmp };

/// What the test asserts.
enum class TestExpectation : u8 {
    Reachable,  ///< at least one reply must come back
    Unreachable ///< no reply may come back; used to prove a firewall rule works
};

[[nodiscard]] std::string_view testProtocolName(TestProtocol protocol);
[[nodiscard]] std::string_view testExpectationName(TestExpectation expectation);

/// A connectivity assertion stored in the project.
///
/// The *definition* lives in the domain model because it is project data that
/// must be serialized and undone like anything else. Running it is a separate
/// concern and lives in the `testing` module, which sits above the simulator.
struct NetworkTest {
    TestId id = TestId::generate();
    std::string name = "New test";
    std::string description;

    DeviceId source;

    /// The destination is named either by device - which survives readdressing -
    /// or by a literal address, for targets outside the topology.
    DeviceId destinationDevice;
    std::optional<Ipv4Address> destinationAddress;

    TestProtocol protocol = TestProtocol::Icmp;
    TestExpectation expectation = TestExpectation::Reachable;

    u32 probeCount = 3;
    Duration timeout = seconds(2);
    std::size_t payloadSize = 32;

    bool enabled = true;

    [[nodiscard]] bool hasDestination() const {
        return destinationDevice.isValid() || destinationAddress.has_value();
    }
};

} // namespace tnp::core
