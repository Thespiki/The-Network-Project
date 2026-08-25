#pragma once

#include "core/routing/Route.h"
#include "utilities/Result.h"

#include <optional>
#include <vector>

namespace tnp::core {

/// A device's IPv4 forwarding table.
///
/// Entries are kept ordered by (prefix length desc, administrative distance asc,
/// metric asc). Because of that ordering the first entry that matches a
/// destination *is* the longest-prefix match, so a lookup is a single forward
/// scan with an early exit rather than a full pass plus a best-so-far
/// comparison.
class RoutingTable {
public:
    /// Adds a route. Fails when an identical destination/next-hop/interface
    /// triple from the same source already exists.
    [[nodiscard]] Status add(Route route);

    /// Adds without duplicate checking; used when rebuilding derived routes.
    void addUnchecked(Route route);

    bool remove(RouteId id);
    /// Removes every route matching a destination network, whatever its source.
    std::size_t removeByDestination(const Ipv4Prefix& destination);
    std::size_t removeBySource(RouteSource source);
    std::size_t removeByInterface(InterfaceId interface);
    void clear();

    [[nodiscard]] const std::vector<Route>& routes() const { return routes_; }
    [[nodiscard]] std::size_t size() const { return routes_.size(); }
    [[nodiscard]] bool empty() const { return routes_.empty(); }

    [[nodiscard]] const Route* find(RouteId id) const;

    /// Longest-prefix-match lookup. Returns nullptr when nothing matches.
    [[nodiscard]] const Route* lookup(Ipv4Address destination) const;

    /// Every enabled route that covers `destination`, best first. Used by the
    /// route-lookup trace and by the "why is this unreachable" diagnostics.
    [[nodiscard]] std::vector<const Route*> matches(Ipv4Address destination) const;

    /// The default route, if one is installed.
    [[nodiscard]] const Route* defaultRoute() const;

private:
    void sort();

    std::vector<Route> routes_;
};

} // namespace tnp::core
