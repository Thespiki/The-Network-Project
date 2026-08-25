#include "core/devices/Firewall.h"

#include <format>

namespace tnp::core {

Firewall::Firewall(DeviceId id, std::string name, std::size_t portCount)
    : Ipv4Device(id, std::move(name)) {
    createInterfaces(InterfaceType::GigabitEthernet, portCount, "0/", 0);

    stack_.setForwardingEnabled(true);
    bindDhcpClient();
    installFilter();
}

void Firewall::installFilter() {
    stack_.setForwardingFilter([this](DeviceContext& context, Interface& ingress, Interface& egress,
                                      const proto::Ipv4Header& header, std::span<const u8> payload) {
        const FirewallPolicy::Evaluation evaluation = policy_.evaluate(header, payload);
        const bool permitted = evaluation.action == FirewallAction::Permit;

        const std::string rule = evaluation.rule
                                     ? (evaluation.rule->name.empty() ? evaluation.rule->toString()
                                                                      : evaluation.rule->name)
                                     : std::format("default {}", firewallActionName(evaluation.action));

        context.trace(TraceEvent{
            .kind = permitted ? TraceKind::FirewallPermitted : TraceKind::FirewallDenied,
            .time = context.now(),
            .device = id(),
            .interface = permitted ? egress.id() : ingress.id(),
            .summary = std::format("{} {} -> {} ({})", permitted ? "permitted" : "denied",
                                   header.source.toString(), header.destination.toString(), rule)}
                .with("source-ip", header.source.toString())
                .with("destination-ip", header.destination.toString())
                .with("protocol", proto::ipProtocolName(header.protocol))
                .with("rule", rule)
                .with("action", std::string{firewallActionName(evaluation.action)}));

        return permitted;
    });
}

void Firewall::onReset() {
    Ipv4Device::onReset();
    policy_.resetCounters();
}

} // namespace tnp::core
