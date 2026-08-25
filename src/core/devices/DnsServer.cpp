#include "core/devices/DnsServer.h"

#include "core/network/Device.h"
#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>

namespace tnp::core {

using namespace proto;

DnsServer::DnsServer(Device& owner, Ipv4Stack& stack) : owner_(owner), stack_(stack) {}

void DnsServer::addRecord(DnsRecord record) {
    if (!record.id.isValid()) record.id = DnsRecordId::generate();
    records_.push_back(std::move(record));
}

bool DnsServer::removeRecord(DnsRecordId id) {
    const auto it = std::find_if(records_.begin(), records_.end(),
                                 [id](const DnsRecord& record) { return record.id == id; });
    if (it == records_.end()) return false;
    records_.erase(it);
    return true;
}

void DnsServer::setRecords(std::vector<DnsRecord> records) {
    records_ = std::move(records);
    for (auto& record : records_) {
        if (!record.id.isValid()) record.id = DnsRecordId::generate();
    }
}

std::optional<Ipv4Address> DnsServer::resolve(std::string_view name) const {
    // Trailing dots are equivalent: "host.local." and "host.local" name the
    // same node.
    std::string wanted = strings::trim(name);
    if (!wanted.empty() && wanted.back() == '.') wanted.pop_back();

    for (const DnsRecord& record : records_) {
        if (strings::equalsIgnoreCase(record.name, wanted)) return record.address;
    }
    return std::nullopt;
}

void DnsServer::handleDatagram(DeviceContext& context, Interface& ingress, const Ipv4Header& ip,
                               const UdpHeader& udp, std::span<const u8> payload) {
    if (!enabled_) return;

    const auto query = decodeDns(payload);
    if (!query || query->isResponse || query->questions.empty()) return;

    const DnsQuestion& question = query->questions.front();

    context.trace(TraceEvent{.kind = TraceKind::DnsQuerySent,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .summary = std::format("query for {} {} from {}", question.name,
                                                    dnsRecordTypeName(question.type),
                                                    ip.source.toString())}
                      .with("name", question.name)
                      .with("type", std::string{dnsRecordTypeName(question.type)})
                      .with("client-ip", ip.source.toString()));

    DnsMessage response;
    response.transactionId = query->transactionId;
    response.isResponse = true;
    response.authoritative = true;
    response.recursionDesired = query->recursionDesired;
    response.questions = query->questions;

    const auto address = question.type == static_cast<u16>(DnsRecordType::A)
                             ? resolve(question.name)
                             : std::nullopt;

    if (address) {
        DnsAnswer answer;
        answer.name = question.name;
        answer.type = static_cast<u16>(DnsRecordType::A);
        answer.address = *address;
        response.answers.push_back(answer);

        context.trace(TraceEvent{.kind = TraceKind::DnsNameResolved,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .summary = std::format("{} resolves to {}", question.name,
                                                        address->toString())}
                          .with("name", question.name)
                          .with("address", address->toString()));
    } else {
        response.responseCode = DnsResponseCode::NameError;

        context.trace(TraceEvent{.kind = TraceKind::DnsNameNotFound,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .summary = std::format("{} is not in this zone", question.name)}
                          .with("name", question.name));
    }

    Ipv4SendOptions options;
    options.egress = ingress.id();
    options.source = ip.destination;

    stack_.sendUdp(context, ip.source, kPortDns, udp.sourcePort, encodeDns(response),
                   FrameCategory::Dns,
                   std::format("DNS response for {}", question.name), options);

    context.trace(TraceEvent{.kind = TraceKind::DnsResponseSent,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .summary = std::format("answered {} with {}", question.name,
                                                    address ? address->toString()
                                                            : std::string{"NXDOMAIN"})}
                      .with("name", question.name)
                      .with("result", address ? address->toString() : std::string{"NXDOMAIN"}));
}

} // namespace tnp::core
