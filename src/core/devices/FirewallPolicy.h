#pragma once

#include "core/network/Ids.h"
#include "core/network/Subnet.h"
#include "core/protocols/Ipv4.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::core {

enum class FirewallAction : u8 { Permit, Deny };

/// Protocol a rule matches on.
enum class FirewallProtocolMatch : u8 { Any, Icmp, Tcp, Udp };

[[nodiscard]] std::string_view firewallActionName(FirewallAction action);
[[nodiscard]] std::string_view firewallProtocolName(FirewallProtocolMatch protocol);

/// One entry in an ordered access policy.
///
/// An empty `source` or `destination` means "any", the same convention access
/// lists use. Port matching only applies to TCP and UDP.
struct FirewallRule {
    FirewallRuleId id;
    std::string name;
    FirewallAction action = FirewallAction::Deny;
    FirewallProtocolMatch protocol = FirewallProtocolMatch::Any;

    std::optional<Ipv4Prefix> source;
    std::optional<Ipv4Prefix> destination;
    std::optional<u16> destinationPortFirst;
    std::optional<u16> destinationPortLast;

    bool enabled = true;
    std::string description;

    /// Number of packets this rule matched during the current run. Runtime
    /// state: reset with the simulation, never serialized.
    u64 hitCount = 0;

    [[nodiscard]] bool matches(const proto::Ipv4Header& header, std::span<const u8> payload) const;

    /// "permit tcp 10.0.0.0/8 any eq 80"
    [[nodiscard]] std::string toString() const;
};

/// An ordered, first-match-wins packet filter.
///
/// The policy only inspects packets; it never modifies them. `Firewall` installs
/// it as the forwarding filter of its IPv4 stack, which is why a firewall routes
/// exactly like a router and differs only in what it refuses to pass.
class FirewallPolicy {
public:
    [[nodiscard]] FirewallAction defaultAction() const { return defaultAction_; }
    void setDefaultAction(FirewallAction action) { defaultAction_ = action; }

    [[nodiscard]] const std::vector<FirewallRule>& rules() const { return rules_; }

    void addRule(FirewallRule rule);
    /// Inserts at `index`, clamped to the end. Order is the policy.
    void insertRule(std::size_t index, FirewallRule rule);
    bool removeRule(FirewallRuleId id);
    bool moveRule(FirewallRuleId id, std::size_t newIndex);
    void setRules(std::vector<FirewallRule> rules);
    void clear();

    /// Result of evaluating a packet.
    struct Evaluation {
        FirewallAction action = FirewallAction::Deny;
        /// The rule that decided, or nullptr when the default action applied.
        const FirewallRule* rule = nullptr;
    };

    /// Evaluates in order and increments the hit counter of the deciding rule.
    Evaluation evaluate(const proto::Ipv4Header& header, std::span<const u8> payload);

    void resetCounters();

private:
    std::vector<FirewallRule> rules_;
    FirewallAction defaultAction_ = FirewallAction::Permit;
};

} // namespace tnp::core
