#pragma once

#include "core/network/Ipv4Address.h"
#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::core::proto {

/// Resource record types TNP encodes.
enum class DnsRecordType : u16 { A = 1, Cname = 5, Ptr = 12, Aaaa = 28 };

/// Response codes (RFC 1035 section 4.1.1).
enum class DnsResponseCode : u8 { NoError = 0, FormatError = 1, ServerFailure = 2, NameError = 3 };

[[nodiscard]] std::string_view dnsRecordTypeName(u16 type);
[[nodiscard]] std::string_view dnsResponseCodeName(DnsResponseCode code);

struct DnsQuestion {
    std::string name;
    u16 type = static_cast<u16>(DnsRecordType::A);
    u16 classCode = 1; ///< IN
};

struct DnsAnswer {
    std::string name;
    u16 type = static_cast<u16>(DnsRecordType::A);
    u16 classCode = 1;
    u32 timeToLive = 300;
    Ipv4Address address; ///< meaningful for A records
};

/// A DNS message.
///
/// Name compression pointers are neither produced nor followed: the simulated
/// zones are tiny, and refusing compression keeps the decoder free of the
/// pointer-loop handling a real resolver needs.
struct DnsMessage {
    u16 transactionId = 0;
    bool isResponse = false;
    bool recursionDesired = true;
    bool authoritative = false;
    DnsResponseCode responseCode = DnsResponseCode::NoError;

    std::vector<DnsQuestion> questions;
    std::vector<DnsAnswer> answers;
};

inline constexpr std::size_t kDnsHeaderSize = 12;

[[nodiscard]] ByteBuffer encodeDns(const DnsMessage& message);
[[nodiscard]] std::optional<DnsMessage> decodeDns(std::span<const u8> bytes);

/// Encodes "www.example.com" as the length-prefixed label sequence.
[[nodiscard]] ByteBuffer encodeDnsName(std::string_view name);

} // namespace tnp::core::proto
