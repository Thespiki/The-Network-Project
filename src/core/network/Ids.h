#pragma once

#include "utilities/Id.h"

namespace tnp::core {

// Phantom tags. Declaring them here (rather than inline in the aliases) keeps
// the identifier types nameable in forward declarations.
struct ProjectTag;
struct DeviceTag;
struct InterfaceTag;
struct LinkTag;
struct PacketTag;
struct TestTag;
struct AnnotationTag;
struct RouteTag;
struct FirewallRuleTag;
struct DhcpPoolTag;
struct DnsRecordTag;

using ProjectId    = Id<ProjectTag>;
using DeviceId     = Id<DeviceTag>;
using InterfaceId  = Id<InterfaceTag>;
using LinkId       = Id<LinkTag>;
using PacketId     = Id<PacketTag>;
using TestId       = Id<TestTag>;
using AnnotationId = Id<AnnotationTag>;
using RouteId        = Id<RouteTag>;
using FirewallRuleId = Id<FirewallRuleTag>;
using DhcpPoolId     = Id<DhcpPoolTag>;
using DnsRecordId    = Id<DnsRecordTag>;

/// Identifies any selectable/reportable object in a project. Validation issues,
/// selection and the properties panel all speak in terms of `ObjectRef` so they
/// do not need one code path per entity kind.
enum class ObjectKind : u8 { None, Device, Interface, Link, Annotation, Test, Route, Packet };

struct ObjectRef {
    ObjectKind kind = ObjectKind::None;
    Uuid id{};

    /// The owning device, for interfaces and routes. Nil otherwise.
    Uuid parent{};

    ObjectRef() = default;
    ObjectRef(ObjectKind k, Uuid identifier, Uuid parentId = {})
        : kind(k), id(identifier), parent(parentId) {}

    static ObjectRef device(DeviceId id) { return {ObjectKind::Device, id.uuid()}; }
    static ObjectRef link(LinkId id) { return {ObjectKind::Link, id.uuid()}; }
    static ObjectRef iface(InterfaceId id, DeviceId owner) {
        return {ObjectKind::Interface, id.uuid(), owner.uuid()};
    }
    static ObjectRef annotation(AnnotationId id) { return {ObjectKind::Annotation, id.uuid()}; }
    static ObjectRef test(TestId id) { return {ObjectKind::Test, id.uuid()}; }

    [[nodiscard]] bool isValid() const { return kind != ObjectKind::None && !id.isNil(); }

    [[nodiscard]] DeviceId asDeviceId() const { return DeviceId{id}; }
    [[nodiscard]] InterfaceId asInterfaceId() const { return InterfaceId{id}; }
    [[nodiscard]] LinkId asLinkId() const { return LinkId{id}; }
    [[nodiscard]] DeviceId parentDeviceId() const { return DeviceId{parent}; }

    bool operator==(const ObjectRef& other) const {
        return kind == other.kind && id == other.id;
    }
};

[[nodiscard]] std::string_view objectKindName(ObjectKind kind);

} // namespace tnp::core

template <>
struct std::hash<tnp::core::ObjectRef> {
    std::size_t operator()(const tnp::core::ObjectRef& ref) const noexcept {
        return std::hash<tnp::Uuid>{}(ref.id) ^ (static_cast<std::size_t>(ref.kind) * 0x9E37u);
    }
};
