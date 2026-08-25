#pragma once

#include "commands/CommandManager.h"
#include "core/project/Project.h"
#include "simulation/Simulator.h"

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::cli {

/// Where in the command hierarchy the session is.
enum class ShellMode : u8 {
    Exec,           ///< Router1#
    Configure,      ///< Router1(config)#
    ConfigInterface ///< Router1(config-if)#
};

/// One line of console output.
struct ShellLine {
    std::string text;
    bool isError = false;
    /// Produced asynchronously by the simulation rather than by a command.
    bool isEvent = false;
};

/// The result of one typed command.
struct ShellResponse {
    std::vector<ShellLine> lines;
    bool isError = false;

    void add(std::string text) { lines.push_back(ShellLine{std::move(text), false, false}); }
    void addError(std::string text) {
        lines.push_back(ShellLine{std::move(text), true, false});
        isError = true;
    }
};

/// A command-line session attached to one simulated device.
///
/// Every command reads or writes the actual model: `show ip route` prints the
/// routing table the forwarding path uses, `ping` starts a real ICMP exchange in
/// the simulator, and configuration commands go through the same undo stack as
/// the graphical editor. Nothing here produces canned text.
class DeviceShell {
public:
    DeviceShell(core::Project& project, sim::Simulator& simulator,
                commands::CommandManager& commands);
    ~DeviceShell();

    DeviceShell(const DeviceShell&) = delete;
    DeviceShell& operator=(const DeviceShell&) = delete;

    /// Attaches the session to a device. Resets the mode to exec.
    void attachTo(core::DeviceId device);
    [[nodiscard]] core::DeviceId attachedDevice() const { return device_; }
    [[nodiscard]] bool isAttached() const;

    /// "Router1#", "Router1(config-if)#"
    [[nodiscard]] std::string prompt() const;
    [[nodiscard]] ShellMode mode() const { return mode_; }

    [[nodiscard]] ShellResponse execute(std::string_view line);

    /// Output produced by the simulation since the last call - ping replies and
    /// timeouts arrive long after the command that started them returned.
    [[nodiscard]] std::vector<ShellLine> drainEvents();

    /// Command history, newest last.
    [[nodiscard]] const std::deque<std::string>& history() const { return history_; }

    /// Completions for the word being typed, in the current mode.
    [[nodiscard]] std::vector<std::string> completions(std::string_view prefix) const;

    /// The device's configuration as command lines, which is what
    /// `show running-config` prints.
    [[nodiscard]] std::vector<std::string> runningConfiguration() const;

private:
    using Words = std::vector<std::string>;

    [[nodiscard]] core::Device* device() const;

    ShellResponse executeExec(const Words& words);
    ShellResponse executeConfigure(const Words& words);
    ShellResponse executeConfigInterface(const Words& words);

    ShellResponse showCommand(const Words& words);
    ShellResponse pingCommand(const Words& words);
    ShellResponse clearCommand(const Words& words);

    ShellResponse showInterfaces(const Words& words);
    ShellResponse showIpInterfaceBrief();
    ShellResponse showIpRoute();
    ShellResponse showArp();
    ShellResponse showMacTable();
    ShellResponse showVlan();
    ShellResponse showDhcpBindings();
    ShellResponse showVersion();
    ShellResponse showRunningConfig();

    /// Turns a ping target into an address: a literal IPv4 address, or the name
    /// of a device in the project (an editor convenience, not name resolution -
    /// the DNS client is not implemented).
    [[nodiscard]] std::optional<core::Ipv4Address> resolveTarget(std::string_view text,
                                                                 std::string& problem) const;

    void onTrace(const core::TraceEvent& event);

    core::Project& project_;
    sim::Simulator& simulator_;
    commands::CommandManager& commands_;

    core::DeviceId device_;
    core::InterfaceId configInterface_;
    ShellMode mode_ = ShellMode::Exec;

    std::deque<std::string> history_;
    std::vector<ShellLine> pending_;
    u32 traceToken_ = 0;
};

} // namespace tnp::cli
