#include "app/TopologyExport.h"

#include "utilities/StringUtilities.h"

#include <format>
#include <limits>
#include <sstream>

namespace tnp::app {
namespace {

using namespace core;

/// Half-extent of a device box, in project units. Matches the canvas so an
/// export looks like what the user was editing.
constexpr float kDeviceHalfWidth = 46.0f;
constexpr float kDeviceHalfHeight = 30.0f;

struct Palette {
    std::string background;
    std::string grid;
    std::string deviceFill;
    std::string deviceStroke;
    std::string text;
    std::string subtleText;
    std::string link;
    std::string linkDown;
};

Palette paletteFor(bool dark) {
    if (dark) {
        return Palette{"#161a20", "#20262e", "#232a34", "#3d4a5c", "#e6eaf0", "#95a2b3",
                       "#5f7d95", "#8a4b4b"};
    }
    return Palette{"#ffffff", "#eef1f5", "#f5f7fa", "#c3ccd8", "#1d232b", "#5d6875", "#7b93a8",
                   "#b06a6a"};
}

/// Accent per device category, so a picture reads at a glance.
std::string accentFor(DeviceType type) {
    switch (deviceTypeCategory(type)) {
        case DeviceCategory::Computers:      return "#4c8bd8";
        case DeviceCategory::Networking:     return "#3fae8f";
        case DeviceCategory::Security:       return "#cf6b5a";
        case DeviceCategory::Wireless:       return "#a173d1";
        case DeviceCategory::Infrastructure: return "#c9a24a";
    }
    return "#4c8bd8";
}

/// The primary address shown under a device, if it has one.
std::string primaryAddressOf(const Device& device) {
    for (const auto& iface : device.interfaces()) {
        if (const auto address = iface->primaryIpv4()) return address->toString();
    }
    return {};
}

} // namespace

Result<std::string> exportTopologySvg(const Project& project, const SvgExportOptions& options) {
    const Network& network = project.network();
    if (network.deviceCount() == 0) {
        return Result<std::string>::failure("the project has no devices to export");
    }

    // Bounding box over placed devices and annotations.
    constexpr float kBig = std::numeric_limits<float>::max();
    Rect bounds{{kBig, kBig}, {-kBig, -kBig}};

    for (const auto& device : network.devices()) {
        const Vec2 position = project.layout().position(device->id());
        bounds.expandToInclude(position - Vec2{kDeviceHalfWidth, kDeviceHalfHeight});
        bounds.expandToInclude(position + Vec2{kDeviceHalfWidth, kDeviceHalfHeight + 26.0f});
    }
    if (options.includeAnnotations) {
        for (const Annotation& annotation : project.annotations()) {
            bounds.expandToInclude(annotation.start);
            bounds.expandToInclude(annotation.end);
        }
    }

    bounds = bounds.expanded(options.margin);
    const float width = bounds.width();
    const float height = bounds.height();

    const Palette palette = paletteFor(options.darkBackground);
    const auto x = [&](float value) { return value - bounds.min.x; };
    const auto y = [&](float value) { return value - bounds.min.y; };

    std::ostringstream out;
    out << std::format(
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="{:.0f}" height="{:.0f}" viewBox="0 0 {:.0f} {:.0f}">)",
        width, height, width, height);
    out << "\n";
    out << std::format(R"(<title>{}</title>)", strings::escapeXml(project.metadata().name)) << "\n";
    out << std::format(R"(<rect width="100%" height="100%" fill="{}"/>)", palette.background) << "\n";
    out << R"(<g font-family="Inter, Segoe UI, Helvetica, Arial, sans-serif">)" << "\n";

    // --- Links, drawn first so devices sit on top ---------------------------
    for (const auto& link : network.links()) {
        const Vec2 a = project.layout().position(link->endpointA().device);
        const Vec2 b = project.layout().position(link->endpointB().device);

        const std::string stroke = link->isEnabled() ? palette.link : palette.linkDown;
        const std::string dash = link->isEnabled() ? "" : R"( stroke-dasharray="6 5")";

        out << std::format(
            R"(<line x1="{:.1f}" y1="{:.1f}" x2="{:.1f}" y2="{:.1f}" stroke="{}" stroke-width="2"{}/>)",
            x(a.x), y(a.y), x(b.x), y(b.y), stroke, dash)
            << "\n";

        if (options.includeInterfaceLabels) {
            const Interface* interfaceA = network.findInterface(link->endpointA().interface);
            const Interface* interfaceB = network.findInterface(link->endpointB().interface);

            // Labels sit a fixed distance from each end, along the cable.
            const Vec2 direction = (b - a).normalized();
            const Vec2 labelA = a + direction * (kDeviceHalfWidth + 18.0f);
            const Vec2 labelB = b - direction * (kDeviceHalfWidth + 18.0f);

            if (interfaceA != nullptr) {
                out << std::format(
                    R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="9" text-anchor="middle">{}</text>)",
                    x(labelA.x), y(labelA.y), palette.subtleText,
                    strings::escapeXml(interfaceA->shortName()))
                    << "\n";
            }
            if (interfaceB != nullptr) {
                out << std::format(
                    R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="9" text-anchor="middle">{}</text>)",
                    x(labelB.x), y(labelB.y), palette.subtleText,
                    strings::escapeXml(interfaceB->shortName()))
                    << "\n";
            }
        }

        if (!link->label().empty()) {
            const Vec2 middle = lerp(a, b, 0.5f);
            out << std::format(
                R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="10" text-anchor="middle">{}</text>)",
                x(middle.x), y(middle.y) - 6.0f, palette.subtleText,
                strings::escapeXml(link->label()))
                << "\n";
        }
    }

    // --- Annotations --------------------------------------------------------
    if (options.includeAnnotations) {
        for (const Annotation& annotation : project.annotations()) {
            const Rect box = annotation.bounds();
            switch (annotation.kind) {
                case AnnotationKind::Rectangle:
                case AnnotationKind::NetworkLabel:
                    out << std::format(
                        R"(<rect x="{:.1f}" y="{:.1f}" width="{:.1f}" height="{:.1f}" fill="none" stroke="{}" stroke-width="{:.1f}" rx="6"/>)",
                        x(box.min.x), y(box.min.y), box.width(), box.height(), palette.subtleText,
                        annotation.thickness)
                        << "\n";
                    break;
                case AnnotationKind::Ellipse:
                    out << std::format(
                        R"(<ellipse cx="{:.1f}" cy="{:.1f}" rx="{:.1f}" ry="{:.1f}" fill="none" stroke="{}" stroke-width="{:.1f}"/>)",
                        x(box.center().x), y(box.center().y), box.width() / 2.0f,
                        box.height() / 2.0f, palette.subtleText, annotation.thickness)
                        << "\n";
                    break;
                case AnnotationKind::Arrow:
                    out << std::format(
                        R"(<line x1="{:.1f}" y1="{:.1f}" x2="{:.1f}" y2="{:.1f}" stroke="{}" stroke-width="{:.1f}"/>)",
                        x(annotation.start.x), y(annotation.start.y), x(annotation.end.x),
                        y(annotation.end.y), palette.subtleText, annotation.thickness)
                        << "\n";
                    break;
                case AnnotationKind::Text:
                    break;
            }

            if (!annotation.text.empty()) {
                const Vec2 anchor = annotation.kind == AnnotationKind::Text
                                        ? annotation.start
                                        : Vec2{box.min.x + 8.0f, box.min.y + annotation.fontSize + 4.0f};
                out << std::format(
                    R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="{:.0f}">{}</text>)",
                    x(anchor.x), y(anchor.y), palette.text, annotation.fontSize,
                    strings::escapeXml(annotation.text))
                    << "\n";
            }
        }
    }

    // --- Devices ------------------------------------------------------------
    for (const auto& device : network.devices()) {
        const Vec2 position = project.layout().position(device->id());
        const float left = x(position.x) - kDeviceHalfWidth;
        const float top = y(position.y) - kDeviceHalfHeight;

        out << std::format(
            R"(<rect x="{:.1f}" y="{:.1f}" width="{:.1f}" height="{:.1f}" rx="8" fill="{}" stroke="{}" stroke-width="1.5"/>)",
            left, top, kDeviceHalfWidth * 2, kDeviceHalfHeight * 2, palette.deviceFill,
            palette.deviceStroke)
            << "\n";

        // Category stripe along the top edge.
        out << std::format(
            R"(<rect x="{:.1f}" y="{:.1f}" width="{:.1f}" height="4" rx="2" fill="{}"/>)",
            left + 8.0f, top + 6.0f, kDeviceHalfWidth * 2 - 16.0f, accentFor(device->type()))
            << "\n";

        out << std::format(
            R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="12" font-weight="600" text-anchor="middle">{}</text>)",
            x(position.x), y(position.y) + 2.0f, palette.text, strings::escapeXml(device->name()))
            << "\n";

        out << std::format(
            R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="9" text-anchor="middle">{}</text>)",
            x(position.x), y(position.y) + 16.0f, palette.subtleText,
            strings::escapeXml(std::string{device->typeDisplayName()}))
            << "\n";

        if (options.includeAddresses) {
            const std::string address = primaryAddressOf(*device);
            if (!address.empty()) {
                out << std::format(
                    R"(<text x="{:.1f}" y="{:.1f}" fill="{}" font-size="10" font-family="monospace" text-anchor="middle">{}</text>)",
                    x(position.x), y(position.y) + kDeviceHalfHeight + 16.0f, palette.subtleText,
                    strings::escapeXml(address))
                    << "\n";
            }
        }
    }

    out << "</g>\n</svg>\n";
    return out.str();
}

} // namespace tnp::app
