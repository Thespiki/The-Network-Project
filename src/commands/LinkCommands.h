#pragma once

#include "commands/Command.h"

#include <vector>

namespace tnp::commands {

/// Cables two interfaces together.
class ConnectInterfacesCommand final : public Command {
public:
    ConnectInterfacesCommand(core::InterfaceId a, core::InterfaceId b);
    ConnectInterfacesCommand(core::InterfaceId a, core::InterfaceId b, core::LinkMedium medium);

    [[nodiscard]] std::string_view kind() const override { return "connect-interfaces"; }
    [[nodiscard]] std::string label() const override { return "Connect"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    [[nodiscard]] core::LinkId linkId() const { return link_; }

    [[nodiscard]] std::string failureReason() const override { return failure_; }

private:
    core::InterfaceId a_;
    core::InterfaceId b_;
    std::optional<core::LinkMedium> medium_;

    /// Fixed on first execution so redo restores the same link identity.
    core::LinkId link_;
    std::string failure_;
};

/// Removes links.
class DisconnectLinksCommand final : public Command {
public:
    explicit DisconnectLinksCommand(std::vector<core::LinkId> links);

    [[nodiscard]] std::string_view kind() const override { return "disconnect-links"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    std::vector<core::LinkId> links_;
    std::vector<std::unique_ptr<core::Link>> removed_;
};

/// Disables or re-enables a link, which is how a cable fault is simulated.
class SetLinkEnabledCommand final : public Command {
public:
    SetLinkEnabledCommand(core::LinkId link, bool enabled);

    [[nodiscard]] std::string_view kind() const override { return "set-link-enabled"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::LinkId link_;
    bool newValue_;
    bool oldValue_ = true;
};

/// Edits a link's label and timing.
class ConfigureLinkCommand final : public Command {
public:
    ConfigureLinkCommand(core::LinkId link, std::string label, Duration propagationDelay,
                         u64 bandwidthMbps);

    [[nodiscard]] std::string_view kind() const override { return "configure-link"; }
    [[nodiscard]] std::string label() const override { return "Configure link"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::LinkId link_;
    std::string newLabel_;
    Duration newDelay_;
    u64 newBandwidth_;

    std::string oldLabel_;
    Duration oldDelay_ = Duration::zero();
    u64 oldBandwidth_ = 0;
};

} // namespace tnp::commands
