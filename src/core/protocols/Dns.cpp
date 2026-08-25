#include "core/protocols/Dns.h"

#include "utilities/ByteStream.h"
#include "utilities/StringUtilities.h"

#include <format>
#include <utility>

namespace tnp::core::proto {
namespace {

constexpr u16 kFlagResponse = 0x8000;
constexpr u16 kFlagAuthoritative = 0x0400;
constexpr u16 kFlagRecursionDesired = 0x0100;
constexpr u8 kMaxLabelLength = 63;

/// Reads a label sequence. Compression pointers are rejected rather than
/// followed; see the note on `DnsMessage`.
std::optional<std::string> readName(ByteReader& reader) {
    std::string name;
    while (true) {
        const auto length = reader.u8v();
        if (!length) return std::nullopt;
        if (*length == 0) break;
        if ((*length & 0xC0u) != 0) return std::nullopt; // compression pointer
        if (*length > kMaxLabelLength) return std::nullopt;

        const auto label = reader.bytes(*length);
        if (!label) return std::nullopt;
        if (!name.empty()) name += '.';
        name.append(reinterpret_cast<const char*>(label->data()), label->size());
    }
    return name;
}

} // namespace

std::string_view dnsRecordTypeName(u16 type) {
    switch (static_cast<DnsRecordType>(type)) {
        case DnsRecordType::A:     return "A";
        case DnsRecordType::Cname: return "CNAME";
        case DnsRecordType::Ptr:   return "PTR";
        case DnsRecordType::Aaaa:  return "AAAA";
    }
    return "TYPE";
}

std::string_view dnsResponseCodeName(DnsResponseCode code) {
    switch (code) {
        case DnsResponseCode::NoError:       return "NOERROR";
        case DnsResponseCode::FormatError:   return "FORMERR";
        case DnsResponseCode::ServerFailure: return "SERVFAIL";
        case DnsResponseCode::NameError:     return "NXDOMAIN";
    }
    return "NOERROR";
}

ByteBuffer encodeDnsName(std::string_view name) {
    ByteBuffer bytes;
    ByteWriter writer{bytes};
    for (const auto& label : strings::split(name, '.', true)) {
        const auto length = static_cast<u8>(label.size() > kMaxLabelLength ? kMaxLabelLength : label.size());
        writer.u8v(length);
        for (std::size_t i = 0; i < length; ++i) writer.u8v(static_cast<u8>(label[i]));
    }
    writer.u8v(0); // root label
    return bytes;
}

ByteBuffer encodeDns(const DnsMessage& message) {
    ByteBuffer bytes;
    ByteWriter writer{bytes};

    u16 flags = 0;
    if (message.isResponse) flags |= kFlagResponse;
    if (message.authoritative) flags |= kFlagAuthoritative;
    if (message.recursionDesired) flags |= kFlagRecursionDesired;
    flags = static_cast<u16>(flags | (static_cast<u16>(message.responseCode) & 0x000Fu));

    writer.u16v(message.transactionId);
    writer.u16v(flags);
    writer.u16v(static_cast<u16>(message.questions.size()));
    writer.u16v(static_cast<u16>(message.answers.size()));
    writer.u16v(0); // authority records
    writer.u16v(0); // additional records

    for (const auto& question : message.questions) {
        writer.bytes(encodeDnsName(question.name));
        writer.u16v(question.type);
        writer.u16v(question.classCode);
    }

    for (const auto& answer : message.answers) {
        writer.bytes(encodeDnsName(answer.name));
        writer.u16v(answer.type);
        writer.u16v(answer.classCode);
        writer.u32v(answer.timeToLive);
        writer.u16v(4); // RDLENGTH for an A record
        writer.u32v(answer.address.value());
    }

    return bytes;
}

std::optional<DnsMessage> decodeDns(std::span<const u8> bytes) {
    if (bytes.size() < kDnsHeaderSize) return std::nullopt;

    ByteReader reader{bytes};
    DnsMessage message;

    const auto transactionId = reader.u16v();
    const auto flags = reader.u16v();
    const auto questionCount = reader.u16v();
    const auto answerCount = reader.u16v();
    const auto authorityCount = reader.u16v();
    const auto additionalCount = reader.u16v();
    if (!transactionId || !flags || !questionCount || !answerCount || !authorityCount || !additionalCount) {
        return std::nullopt;
    }

    message.transactionId = *transactionId;
    message.isResponse = (*flags & kFlagResponse) != 0;
    message.authoritative = (*flags & kFlagAuthoritative) != 0;
    message.recursionDesired = (*flags & kFlagRecursionDesired) != 0;
    message.responseCode = static_cast<DnsResponseCode>(*flags & 0x000Fu);

    for (u16 i = 0; i < *questionCount; ++i) {
        const auto name = readName(reader);
        const auto type = reader.u16v();
        const auto classCode = reader.u16v();
        if (!name || !type || !classCode) return std::nullopt;
        message.questions.push_back(DnsQuestion{*name, *type, *classCode});
    }

    for (u16 i = 0; i < *answerCount; ++i) {
        const auto name = readName(reader);
        const auto type = reader.u16v();
        const auto classCode = reader.u16v();
        const auto timeToLive = reader.u32v();
        const auto dataLength = reader.u16v();
        if (!name || !type || !classCode || !timeToLive || !dataLength) return std::nullopt;

        const auto data = reader.bytes(*dataLength);
        if (!data) return std::nullopt;

        DnsAnswer answer;
        answer.name = *name;
        answer.type = *type;
        answer.classCode = *classCode;
        answer.timeToLive = *timeToLive;
        if (*type == static_cast<u16>(DnsRecordType::A) && data->size() == 4) {
            const auto address = Ipv4Address::fromBytes(*data);
            if (address) answer.address = *address;
        }
        message.answers.push_back(std::move(answer));
    }

    return message;
}

} // namespace tnp::core::proto
