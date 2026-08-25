#include "core/protocols/Arp.h"
#include "core/protocols/Ethernet.h"
#include "core/protocols/Icmp.h"
#include "core/protocols/Ipv4.h"
#include "simulation/Packet.h"
#include "simulation/PacketDecoder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::core::proto;

namespace {

MacAddress mac(const char* text) { return *MacAddress::parse(text); }
Ipv4Address ipv4(const char* text) { return *Ipv4Address::parse(text); }

ByteBuffer makeIcmpFrame(u8 ttl = 64) {
    Ipv4Header ip;
    ip.source = ipv4("192.168.1.10");
    ip.destination = ipv4("172.16.0.20");
    ip.ttl = ttl;
    ip.protocol = static_cast<u8>(IpProtocol::Icmp);

    EthernetHeader ethernet;
    ethernet.destination = mac("AA:BB:CC:DD:EE:FF");
    ethernet.source = mac("11:22:33:44:55:66");
    ethernet.etherType = static_cast<u16>(EtherType::Ipv4);

    return encodeEthernet(ethernet,
                          encodeIpv4(ip, encodeIcmpEcho(IcmpType::EchoRequest, 0x1234, 7,
                                                        ByteBuffer(32, 0x61))));
}

const sim::DecodedField* findField(const sim::DecodedLayer& layer, std::string_view name) {
    const auto it = std::find_if(layer.fields.begin(), layer.fields.end(),
                                 [name](const sim::DecodedField& field) { return field.name == name; });
    return it == layer.fields.end() ? nullptr : &*it;
}

} // namespace

TEST_CASE("A ping frame decodes into three layers", "[simulation][decoder]") {
    const sim::DecodedPacket decoded = sim::decodePacket(makeIcmpFrame());

    REQUIRE(decoded.layers.size() == 3);
    CHECK(decoded.layers[0].name == "Ethernet II");
    CHECK(decoded.layers[1].name == "IPv4");
    CHECK(decoded.layers[2].name == "ICMP");
    CHECK(decoded.problem.empty());
    CHECK(decoded.summary.find("Echo Request") != std::string::npos);

    const sim::DecodedField* ttl = findField(decoded.layers[1], "TTL");
    REQUIRE(ttl != nullptr);
    CHECK(ttl->value == "64");

    const sim::DecodedField* checksum = findField(decoded.layers[1], "Checksum");
    REQUIRE(checksum != nullptr);
    CHECK(checksum->detail == "correct");

    const sim::DecodedField* identifier = findField(decoded.layers[2], "Identifier");
    REQUIRE(identifier != nullptr);
    CHECK(identifier->value == "4660"); // 0x1234
}

TEST_CASE("The decoder reports a bad checksum rather than hiding it", "[simulation][decoder]") {
    ByteBuffer frame = makeIcmpFrame();
    frame[14 + 8] = 1; // rewrite the TTL without repairing the checksum

    const sim::DecodedPacket decoded = sim::decodePacket(frame);
    REQUIRE(decoded.layers.size() >= 2);

    const sim::DecodedField* checksum = findField(decoded.layers[1], "Checksum");
    REQUIRE(checksum != nullptr);
    CHECK(checksum->detail == "INCORRECT");
}

TEST_CASE("ARP frames decode into a readable question", "[simulation][decoder]") {
    EthernetHeader ethernet;
    ethernet.destination = MacAddress::broadcast();
    ethernet.source = mac("11:22:33:44:55:66");
    ethernet.etherType = static_cast<u16>(EtherType::Arp);

    const ByteBuffer frame = encodeEthernet(
        ethernet, encodeArp(makeArpRequest(mac("11:22:33:44:55:66"), ipv4("192.168.1.10"),
                                           ipv4("192.168.1.1"))));

    const sim::DecodedPacket decoded = sim::decodePacket(frame);
    REQUIRE(decoded.layers.size() == 2);
    CHECK(decoded.layers[1].name == "ARP");
    CHECK(decoded.layers[1].summary == "Who has 192.168.1.1? Tell 192.168.1.10");
    CHECK(findField(decoded.layers[0], "Destination")->detail == "broadcast");
}

TEST_CASE("Frames are classified by protocol", "[simulation][decoder]") {
    CHECK(sim::classifyFrame(makeIcmpFrame()) == FrameCategory::Icmp);
    CHECK(sim::classifyFrame(ByteBuffer{}) == FrameCategory::Unknown);

    EthernetHeader ethernet;
    ethernet.destination = MacAddress::broadcast();
    ethernet.source = mac("11:22:33:44:55:66");
    ethernet.etherType = static_cast<u16>(EtherType::Arp);
    const ByteBuffer arpFrame = encodeEthernet(
        ethernet, encodeArp(makeArpRequest(mac("11:22:33:44:55:66"), ipv4("10.0.0.1"),
                                           ipv4("10.0.0.2"))));
    CHECK(sim::classifyFrame(arpFrame) == FrameCategory::Arp);
}

TEST_CASE("An undecodable buffer reports the problem", "[simulation][decoder]") {
    const sim::DecodedPacket decoded = sim::decodePacket(ByteBuffer(4, 0x00));
    CHECK(decoded.layers.empty());
    CHECK_FALSE(decoded.problem.empty());
}

TEST_CASE("The packet registry keeps identity across re-encapsulation", "[simulation][packets]") {
    sim::PacketRegistry registry;

    Frame frame;
    frame.id = PacketId::generate();
    frame.origin = DeviceId::generate();
    frame.category = FrameCategory::Icmp;
    frame.summary = "ICMP echo request";
    frame.bytes = makeIcmpFrame(64);

    registry.observe(frame);
    CHECK(registry.size() == 1);

    // A router rewrites the bytes but the packet is the same packet.
    frame.bytes = makeIcmpFrame(63);
    registry.observe(frame);
    CHECK(registry.size() == 1);

    const sim::PacketRecord* record = registry.find(frame.id);
    REQUIRE(record != nullptr);
    CHECK(sim::decodePacket(record->bytes).layers[1].fields[5].value == "63");

    registry.addHop(frame.id, sim::PacketHop{SimTime{}, frame.origin, InterfaceId{}, "forwarded", ""});
    CHECK(registry.find(frame.id)->hops.size() == 1);
}

TEST_CASE("The packet registry drops the oldest records past its capacity",
          "[simulation][packets]") {
    sim::PacketRegistry registry;
    registry.setCapacity(3);

    std::vector<PacketId> ids;
    for (int i = 0; i < 5; ++i) {
        Frame frame;
        frame.id = PacketId::generate();
        frame.bytes = makeIcmpFrame();
        ids.push_back(frame.id);
        registry.observe(frame);
    }

    CHECK(registry.size() == 3);
    CHECK(registry.find(ids[0]) == nullptr);
    CHECK(registry.find(ids[1]) == nullptr);
    CHECK(registry.find(ids[4]) != nullptr);
}

TEST_CASE("In-flight progress interpolates between departure and arrival",
          "[simulation][packets]") {
    sim::PacketInFlight flight;
    flight.departure = SimTime{milliseconds(100)};
    flight.arrival = SimTime{milliseconds(200)};

    CHECK(flight.progressAt(SimTime{milliseconds(100)}) == 0.0f);
    CHECK(flight.progressAt(SimTime{milliseconds(150)}) == 0.5f);
    CHECK(flight.progressAt(SimTime{milliseconds(200)}) == 1.0f);
    CHECK(flight.progressAt(SimTime{milliseconds(50)}) == 0.0f);
    CHECK(flight.progressAt(SimTime{milliseconds(500)}) == 1.0f);

    // A zero-length flight is simply complete.
    flight.arrival = flight.departure;
    CHECK(flight.progressAt(flight.departure) == 1.0f);
}
