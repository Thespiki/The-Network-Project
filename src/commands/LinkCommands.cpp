#include "commands/LinkCommands.h"

#include <format>

namespace tnp::commands {

using namespace core;

// --- ConnectInterfacesCommand ----------------------------------------------

ConnectInterfacesCommand::ConnectInterfacesCommand(InterfaceId a, InterfaceId b) : a_(a), b_(b) {}

ConnectInterfacesCommand::ConnectInterfacesCommand(InterfaceId a, InterfaceId b, LinkMedium medium)
    : a_(a), b_(b), medium_(medium) {}

bool ConnectInterfacesCommand::execute(Project& project) {
    if (!link_.isValid()) link_ = LinkId::generate();

    Network& network = project.network();

    LinkMedium medium = medium_.value_or(LinkMedium::Copper);
    if (!medium_) {
        // Let the network choose based on the interface types, then remember the
        // choice so redo produces the same link.
        auto probe = network.connect(a_, b_);
        if (!probe) {
            failure_ = probe.message();
            return false;
        }
        if (Link* created = network.findLink(probe.value())) medium = created->medium();
        auto removed = network.disconnect(probe.value());
        (void)removed;
        medium_ = medium;
    }

    auto created = network.connectWithId(link_, a_, b_, medium);
    if (!created) {
        failure_ = created.message();
        return false;
    }
    return true;
}

void ConnectInterfacesCommand::undo(Project& project) {
    auto removed = project.network().disconnect(link_);
    (void)removed;
}

// --- DisconnectLinksCommand ------------------------------------------------

DisconnectLinksCommand::DisconnectLinksCommand(std::vector<LinkId> links) : links_(std::move(links)) {}

std::string DisconnectLinksCommand::label() const {
    return links_.size() == 1 ? "Disconnect" : std::format("Disconnect {} links", links_.size());
}

bool DisconnectLinksCommand::execute(Project& project) {
    removed_.clear();
    for (const LinkId id : links_) {
        if (auto link = project.network().disconnect(id)) removed_.push_back(std::move(link));
    }
    return !removed_.empty();
}

void DisconnectLinksCommand::undo(Project& project) {
    for (auto& link : removed_) project.network().restoreLink(std::move(link));
    removed_.clear();
}

// --- SetLinkEnabledCommand -------------------------------------------------

SetLinkEnabledCommand::SetLinkEnabledCommand(LinkId link, bool enabled)
    : link_(link), newValue_(enabled) {}

std::string SetLinkEnabledCommand::label() const {
    return newValue_ ? "Enable link" : "Disable link";
}

bool SetLinkEnabledCommand::execute(Project& project) {
    Link* link = project.network().findLink(link_);
    if (link == nullptr || link->isEnabled() == newValue_) return false;

    oldValue_ = link->isEnabled();
    link->setEnabled(newValue_);
    project.network().refreshOperationalStates();
    return true;
}

void SetLinkEnabledCommand::undo(Project& project) {
    if (Link* link = project.network().findLink(link_)) {
        link->setEnabled(oldValue_);
        project.network().refreshOperationalStates();
    }
}

// --- ConfigureLinkCommand --------------------------------------------------

ConfigureLinkCommand::ConfigureLinkCommand(LinkId link, std::string label, Duration propagationDelay,
                                           u64 bandwidthMbps)
    : link_(link), newLabel_(std::move(label)), newDelay_(propagationDelay),
      newBandwidth_(bandwidthMbps) {}

bool ConfigureLinkCommand::execute(Project& project) {
    Link* link = project.network().findLink(link_);
    if (link == nullptr) return false;

    const bool unchanged = link->label() == newLabel_ && link->propagationDelay() == newDelay_ &&
                           link->bandwidthMbps() == newBandwidth_;
    if (unchanged) return false;

    oldLabel_ = link->label();
    oldDelay_ = link->propagationDelay();
    oldBandwidth_ = link->bandwidthMbps();

    link->setLabel(newLabel_);
    link->setPropagationDelay(newDelay_);
    link->setBandwidthMbps(newBandwidth_);
    return true;
}

void ConfigureLinkCommand::undo(Project& project) {
    if (Link* link = project.network().findLink(link_)) {
        link->setLabel(oldLabel_);
        link->setPropagationDelay(oldDelay_);
        link->setBandwidthMbps(oldBandwidth_);
    }
}

} // namespace tnp::commands
