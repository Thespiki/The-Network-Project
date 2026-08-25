#include "core/devices/FirewallPolicy.h"

#include "core/protocols/Tcp.h"
#include "core/protocols/Udp.h"

#include <algorithm>
#include <format>
#include <utility>

namespace tnp::core {
namespace {

/// Destination port of a TCP or UDP payload, when one can be read.
std::optional<u16> destinationPortOf(u8 protocol, std::span<const u8> payload) {
    switch (static_cast<proto::IpProtocol>(protocol)) {
        case proto::IpProtocol::Udp:
            if (const auto datagram = proto::decodeUdp(payload)) return datagram->header.destinationPort;
            return std::nullopt;
        case proto::IpProtocol::Tcp:
            if (const auto segment = proto::decodeTcp(payload)) return segment->header.destinationPort;
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

bool protocolMatches(FirewallProtocolMatch match, u8 protocol) {
    switch (match) {
        case FirewallProtocolMatch::Any:  return true;
        case FirewallProtocolMatch::Icmp: return protocol == static_cast<u8>(proto::IpProtocol::Icmp);
        case FirewallProtocolMatch::Tcp:  return protocol == static_cast<u8>(proto::IpProtocol::Tcp);
        case FirewallProtocolMatch::Udp:  return protocol == static_cast<u8>(proto::IpProtocol::Udp);
    }
    return false;
}

std::string prefixText(const std::optional<Ipv4Prefix>& prefix) {
    return prefix ? prefix->toNetworkString() : std::string{"any"};
}

} // namespace

std::string_view firewallActionName(FirewallAction action) {
    return action == FirewallAction::Permit ? "permit" : "deny";
}

std::string_view firewallProtocolName(FirewallProtocolMatch protocol) {
    switch (protocol) {
        case FirewallProtocolMatch::Any:  return "ip";
        case FirewallProtocolMatch::Icmp: return "icmp";
        case FirewallProtocolMatch::Tcp:  return "tcp";
        case FirewallProtocolMatch::Udp:  return "udp";
    }
    return "ip";
}

bool FirewallRule::matches(const proto::Ipv4Header& header, std::span<const u8> payload) const {
    if (!enabled) return false;
    if (!protocolMatches(protocol, header.protocol)) return false;
    if (source && !source->contains(header.source)) return false;
    if (destination && !destination->contains(header.destination)) return false;

    if (destinationPortFirst || destinationPortLast) {
        const auto port = destinationPortOf(header.protocol, payload);
        if (!port) return false; // a port range cannot match a protocol without ports
        const u16 first = destinationPortFirst.value_or(0);
        const u16 last = destinationPortLast.value_or(destinationPortFirst.value_or(65535));
        if (*port < first || *port > last) return false;
    }
    return true;
}

std::string FirewallRule::toString() const {
    std::string text = std::format("{} {} {} {}", firewallActionName(action),
                                   firewallProtocolName(protocol), prefixText(source),
                                   prefixText(destination));
    if (destinationPortFirst && destinationPortLast && *destinationPortFirst != *destinationPortLast) {
        text += std::format(" range {} {}", *destinationPortFirst, *destinationPortLast);
    } else if (destinationPortFirst) {
        text += std::format(" eq {}", *destinationPortFirst);
    }
    if (!enabled) text += " (disabled)";
    return text;
}

void FirewallPolicy::addRule(FirewallRule rule) {
    insertRule(rules_.size(), std::move(rule));
}

void FirewallPolicy::insertRule(std::size_t index, FirewallRule rule) {
    if (!rule.id.isValid()) rule.id = FirewallRuleId::generate();
    index = std::min(index, rules_.size());
    rules_.insert(rules_.begin() + static_cast<std::ptrdiff_t>(index), std::move(rule));
}

bool FirewallPolicy::removeRule(FirewallRuleId id) {
    const auto it = std::find_if(rules_.begin(), rules_.end(),
                                 [id](const FirewallRule& rule) { return rule.id == id; });
    if (it == rules_.end()) return false;
    rules_.erase(it);
    return true;
}

bool FirewallPolicy::moveRule(FirewallRuleId id, std::size_t newIndex) {
    const auto it = std::find_if(rules_.begin(), rules_.end(),
                                 [id](const FirewallRule& rule) { return rule.id == id; });
    if (it == rules_.end()) return false;

    FirewallRule rule = std::move(*it);
    rules_.erase(it);
    newIndex = std::min(newIndex, rules_.size());
    rules_.insert(rules_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(rule));
    return true;
}

void FirewallPolicy::setRules(std::vector<FirewallRule> rules) {
    rules_ = std::move(rules);
    for (auto& rule : rules_) {
        if (!rule.id.isValid()) rule.id = FirewallRuleId::generate();
    }
}

void FirewallPolicy::clear() { rules_.clear(); }

FirewallPolicy::Evaluation FirewallPolicy::evaluate(const proto::Ipv4Header& header,
                                                    std::span<const u8> payload) {
    for (FirewallRule& rule : rules_) {
        if (!rule.matches(header, payload)) continue;
        ++rule.hitCount;
        return Evaluation{rule.action, &rule};
    }
    return Evaluation{defaultAction_, nullptr};
}

void FirewallPolicy::resetCounters() {
    for (FirewallRule& rule : rules_) rule.hitCount = 0;
}

} // namespace tnp::core
