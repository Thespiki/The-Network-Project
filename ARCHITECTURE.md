# TNP architecture

This document explains how TNP is put together and, more usefully, *why*. It assumes you
have read the README.

---

## The layers

Each directory under `src/` is a separate CMake target. The dependency arrow points one way
and the build enforces it:

```text
                    ┌──────────┐
                    │    ui    │  Dear ImGui workspace
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │   app    │  document lifecycle, autosave, export, learning mode
                    └────┬─────┘
                         │
   ┌──────────┬──────────┼──────────┬────────────┬──────────────┐
   │          │          │          │            │              │
┌──▼───┐ ┌───▼────┐ ┌───▼─────┐ ┌──▼───────┐ ┌──▼──────────┐ ┌─▼───────────┐
│ cli  │ │ testing│ │commands │ │validation│ │ simulation  │ │serialization│
└──┬───┘ └───┬────┘ └───┬─────┘ └──┬───────┘ └──┬──────────┘ └─┬───────────┘
   └─────────┴──────────┴──────────┴────────────┴──────────────┘
                         │
                    ┌────▼─────┐
                    │   core   │  the domain model
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │utilities │  identifiers, time, logging, bytes, files
                    └──────────┘
```

Three consequences follow, and they are the point of the whole arrangement:

- **`core` cannot include a simulator header.** Device behaviour is therefore written without
  any knowledge of scheduling, which is what makes it testable in isolation.
- **Nothing below `ui` can include Dear ImGui.** The engine runs identically in the window, in
  `tnpcli` and in the test suite. There is no rendering path that produces different results.
- **`app` is the only thing that knows about a document.** `Application` owns the project, the
  undo stack, the simulator and the validator, and the window is a client of it. The headless
  tool drives the same object.

---

## The seam between core and simulation

This is the interesting problem. Device behaviour belongs in the domain model; time, links
and queueing belong to the simulator. Devices need the simulator's services, but `core`
must not depend on `simulation`.

The answer is dependency inversion. `core/network/DeviceContext.h` declares an interface:

```cpp
class DeviceContext {
public:
    virtual SimTime now() const = 0;
    virtual Frame makeFrame(const Device& origin, ByteBuffer bytes, FrameCategory, std::string) = 0;
    virtual void transmit(Device& sender, Interface& out, Frame frame) = 0;
    virtual void loopback(Device& device, Interface& iface, Frame frame) = 0;
    virtual void scheduleTimer(Device& device, TimerId timer, Duration delay) = 0;
    virtual void trace(TraceEvent event) = 0;
    // ...
};
```

`sim::Simulator` implements it. A device therefore knows how to ask for a frame to go out of
*one of its own interfaces* and nothing more: it has never heard of a link, a queue, or a
clock that is not handed to it.

Two things fall out of this that are worth noticing.

**Loopback goes through the event queue.** When a host pings its own address, the frame is
wrapped in an Ethernet header addressed to itself and handed to `DeviceContext::loopback`,
which schedules it like any other arrival. A direct call would have been simpler and would
have recursed without bound the first time somebody built a loop.

**A layer-3 switch wraps the context.** An SVI has no cable. When the IP stack transmits on
one, `Layer3Switch` passes a `VlanEgressContext` that intercepts exactly that case and hands
the frame to the switching engine, which picks a real port from the forwarding database or
floods. Every other call passes straight through. The IP stack does not know it is running on
a switch, and the switching engine does not know the frame came from inside the device.

---

## Behaviour is composed, not inherited

The device hierarchy is one level deep. `Device` is abstract; `Ipv4Device` adds the receive,
timer and reset plumbing shared by everything with an IP stack; the nine concrete types sit
under those.

What makes them different is what they *hold*:

| Device | Composition |
| --- | --- |
| `Pc` | `Ipv4Stack` (forwarding off) + `DhcpClient` |
| `Router` | `Ipv4Stack` (forwarding on) + `DhcpClient` + `DhcpServer` |
| `Firewall` | the same as a router, plus a `FirewallPolicy` installed as the stack's forwarding filter |
| `Switch` | `SwitchingEngine` |
| `Layer3Switch` | `SwitchingEngine` + `Ipv4Stack` |
| `Hub` | nothing — it repeats frames, which is the absence of bridging |

There is exactly one implementation of IPv4 forwarding in TNP, and a firewall is a router
with a predicate. That is not a stylistic preference: a second forwarding implementation is
how a simulator ends up with a router and a firewall that disagree about the same packet.

Callers ask a device what it can do rather than downcasting:

```cpp
if (Ipv4Stack* stack = device.ipv4Stack()) { /* ... */ }
if (SwitchingEngine* switching = device.switching()) { /* ... */ }
```

---

## Packets are real bytes

`core::Frame` holds a `ByteBuffer` containing a complete Ethernet frame. The protocol
encoders in `core/protocols/` write actual wire format — big-endian fields, correct lengths,
computed checksums — and the decoders read it back with bounds checking, returning
`std::optional` so a truncated packet fails cleanly.

This costs a little performance and buys a great deal:

- The packet inspector shows what is in the buffer, including a checksum that does not
  verify. A field cannot disagree with the packet, because it *is* the packet.
- A router forwarding a packet rewrites the TTL in place and repairs the checksum, exactly
  as hardware does — and a test can assert the checksum is right afterwards.
- An ICMP error quotes the real datagram that caused it, and the sender decodes that quote to
  match the error to the request. That is why a ping to an unrouted address fails
  *immediately* rather than after a timeout.

Ethernet padding is modelled too, which is why an ARP frame is 60 bytes rather than 42, and
why the IPv4 decoder trusts the length field rather than the buffer size.

### Identity across hops

A `Frame` carries a `PacketId` that survives re-encapsulation. A router builds a new Ethernet
header but calls `makeForwardedFrame` with the previous identity, so the inspector shows one
packet crossing the topology rather than a new packet per hop. The hop count advances at
layer 3 and not at layer 2 — the caller decides, because a bridge is not a hop.

---

## The simulation engine

`sim::NetworkScheduler` owns the clock and the event queue; `sim::Simulator` decides what
events *do*. Time moves only when an event is taken off the queue, or when the caller
explicitly advances it. It is never driven by rendering.

Events are ordered by `(time, insertion sequence)`. That total order is what makes a run
reproducible: two frames arriving at the same nanosecond are always processed in the order
they were scheduled, and a test asserts that two runs of the same project produce identical
timelines.

Timers are cancelled with a tombstone set rather than by searching the heap. Timers are
cancelled far more often than they are examined, and a binary heap has no cheap erase.

`advance()` caps how many events one frame may process. A broadcast storm or a routing loop
can generate work faster than time passes; with the cap the simulation falls behind and the
user can pause and look, instead of the window freezing.

---

## Everything observable is a `TraceEvent`

The engine reports what it did through one channel:

```cpp
struct TraceEvent {
    TraceKind kind;
    SimTime time;
    DeviceId device; InterfaceId interface; PacketId packet;
    std::string summary;                 // factual, never explanatory
    std::vector<TraceField> fields;      // structured: "target-ip", "ttl", "next-hop"
    u64 sequence;
};
```

The log panel, the event timeline, the packet inspector, the test runner and learning mode
all consume the same stream. None of them reaches into device internals.

The `fields` are what make learning mode possible without putting explanations in the engine.
`LearningNarrator` reads `kind` and the named fields and builds sentences; the wording lives
in one file and can change — or be translated — without touching a protocol implementation.
That is the difference between a narrated simulator and a simulator with narration hard-coded
into it.

---

## Configuration versus runtime state

This distinction runs through the whole model and is worth stating plainly.

| Configuration (serialized) | Runtime state (rebuilt) |
| --- | --- |
| `StaticRouteEntry` — what the user configured | `RoutingTable` — connected + static + dynamic |
| Interface addresses | ARP cache, MAC table, counters |
| DHCP pools | DHCP leases |
| Firewall rules | Rule hit counts |
| Device positions in `Layout` | Selection, view offset, zoom |

Connected routes are derived from interface state every time it changes. They are never
written to a file, because a saved connected route goes stale the moment an interface is
renumbered.

The same rule explains a subtlety in the serializer: an interface configured for DHCP has its
addresses *skipped* when saving. Whatever it holds during a run was leased, and writing that
would turn a lease into a permanent setting the next time the project opened.

Device positions live in `core::Layout`, keyed by `DeviceId` — a `Device` has no coordinates
at all. Moving an icon must not touch the object the simulator reads, and a topology stays
meaningful when it is generated or diffed with no layout at all.

---

## Identity

Every persisted entity is keyed by a UUID wrapped in a phantom type:

```cpp
using DeviceId    = Id<DeviceTag>;
using InterfaceId = Id<InterfaceTag>;
```

`DeviceId` and `InterfaceId` are distinct types, so passing one where the other is expected
does not compile. Identifiers cross every layer of the application; the type safety pays for
itself.

Identifiers survive serialization, which is what lets undo restore a deleted device *as the
same device*, and lets a stored test still find the devices it names after a save and reload.

---

## Undo/redo

Every editor change is a `Command` that stores only what it needs to reverse itself.
Nothing snapshots the application: with hundreds of devices that would make undo unusable.

`DeleteDevicesCommand` keeps the removed `unique_ptr<Device>` and its links, so undo puts
back the original objects with their configuration and identifiers intact. `MoveDevicesCommand`
merges with the previous one when the selection matches, so a drag that produces a command per
frame is one undo step.

A command that changes nothing returns `false`, is destroyed, and never enters the history —
which is why the failure reason travels through `CommandManager::lastFailure()` rather than
through a pointer the caller would be holding after the object was freed.

The device console issues the same commands the properties panel does. Configuring an
interface from the CLI is undoable, and the two views can never disagree.

---

## Validation

Rules are separate objects implementing `ValidationRule`, not branches in one function, so
each can be tested and disabled individually. They receive the project **read-only**:
validation never repairs anything. A tool that quietly rewrote a user's configuration while
checking it would be considerably more dangerous than one that only reports.

Findings carry a stable `code` alongside the message. Messages are for people; codes are for
filtering, suppressing and testing.

One rule exists purely for honesty: `feature-not-simulated` raises an informational issue when
a project configures OSPF, because this build stores that configuration but does not run it.

---

## Running tests without disturbing the user

`NetworkTestRunner` copies the project through the serializer and runs against the copy.

Two reasons. Starting a simulation resets every device's caches, so running the suite against
the live model would throw away whatever the user was watching. And a test that leaves an ARP
entry behind would change the result of the next one — results would depend on the order tests
happened to run in. Each test gets a fresh simulator with cold caches.

Copying through the serializer also means the round trip is exercised every time the suite
runs.

---

## Where this deviates from the original sketch

Two classes named in the original plan do not exist, and the reasons are worth recording.

**`PacketPipeline` / `ProtocolProcessor`.** The pipeline is real but it is not a class: it is
`Device::onFrameReceived` dispatching into `Ipv4Stack` or `SwitchingEngine`, which call back
through `DeviceContext`. Adding a class in the middle would have been a name with no
behaviour behind it.

**`NetworkTest` lives in `core/project`, not in `testing`.** A test definition is project data:
it is serialized, undoable and validated like anything else. Putting it in `testing` would
have made `core` depend on a layer above it. The *runner* is in `testing`, where it belongs.

---

## Performance

The application targets hundreds of devices and thousands of links.

- `Network` stores devices and links in vectors — ordered, so serialization and simulation are
  reproducible — and maintains hash indices for lookup by identifier, by interface, and by
  device adjacency. Nothing scans the topology to answer "what is attached to this interface".
- The routing table is kept sorted by (prefix length, administrative distance, metric), so the
  first matching entry *is* the longest-prefix match: lookup is a forward scan with an early
  exit rather than a full pass.
- The canvas draws with `ImDrawList` rather than one widget per device, and nothing rebuilds
  the topology per frame. Validation is cached and recomputed only when a command reports a
  change.
- Trace and packet histories are bounded, with the trace log trimmed in blocks so a long run
  does not degrade into repeated front-erasure of a vector.

---

## Error handling

TNP does not throw across module boundaries. `Result<T>` and `Status` carry failures, and
`[[nodiscard]]` makes ignoring one a warning.

Loading a project is deliberately asymmetric. **Structural** problems — invalid JSON, a
document that is not a project, an unreadable major version — fail the load before the
existing project is touched. **Field-level** problems — a malformed address, an unknown device
type, a link pointing at an interface that is not there — cost that field and produce a
warning. A project with one bad address should still open, with the problem reported, rather
than refusing to load at all.

The `.tnp` container verifies a magic number, a version and a CRC-32 per entry, and refuses
implausible lengths before allocating. A corrupt file is reported, never partially loaded.
