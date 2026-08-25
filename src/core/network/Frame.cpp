#include "core/network/Frame.h"

namespace tnp::core {

std::string_view frameCategoryName(FrameCategory category) {
    switch (category) {
        case FrameCategory::Unknown:   return "Unknown";
        case FrameCategory::Arp:       return "ARP";
        case FrameCategory::Icmp:      return "ICMP";
        case FrameCategory::IcmpError: return "ICMP Error";
        case FrameCategory::Tcp:       return "TCP";
        case FrameCategory::Udp:       return "UDP";
        case FrameCategory::Dhcp:      return "DHCP";
        case FrameCategory::Dns:       return "DNS";
        case FrameCategory::Ipv6:      return "IPv6";
        case FrameCategory::Other:     return "Other";
    }
    return "Unknown";
}

} // namespace tnp::core
