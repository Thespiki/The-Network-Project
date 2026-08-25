# The Network Project (TNP)

**TNP** is a desktop application for designing, configuring, simulating and analysing
computer networks. It combines a topology editor, an event-driven network simulator, a
packet analyser and a diagnostics environment in one program.

> Design → Configure → Simulate → Inspect → Diagnose → Save

Packets in TNP carry **real bytes**. Headers are encoded to the wire format, checksums are
computed and verified, and the inspector decodes what is actually in the buffer — including
a checksum that does not verify. Nothing is faked: `show ip route` prints the table the
forwarding path uses, and the dot moving across the canvas is a frame the engine really
scheduled.

---

## Status

TNP is at version **0.5.0**. Milestones 1–5 of the plan are complete and milestone 6 is
partly done. The whole of the intended workflow works end to end today:

```text
new project → drag devices → cable interfaces → assign addresses → add routes
  → validate → run → ping → watch ARP and the frames cross → inspect a packet
  → run the automated tests → save → reload → keep working
```

That path is covered by an integration test (`tests/integration/test_workflow.cpp`), so it
stays working.

**186 tests pass.** See [What is and is not simulated](#what-is-and-is-not-simulated) for an
honest account of the boundaries.

---

## Features

### Topology editor

- Pan, zoom, grid, optional snapping, minimap
- Click, shift/ctrl-click, and box selection across devices, links and annotations
- Drag from the palette, drag to move, arrow keys to nudge
- Copy and paste, align, distribute, lock position
- Annotations: text, rectangles, ellipses, arrows and network-zone labels
- Interface-level cabling with an interface picker, plus per-end link status
- Undo and redo for every editor change, with drag gestures merged into one step

### Devices

| Device | What it does |
| --- | --- |
| PC | IPv4 host: ARP cache, routing table, DHCP client, ping |
| Server | A PC plus a DHCP server and an authoritative DNS zone |
| Router | IPv4 forwarding with longest-prefix match, ICMP errors, DHCP server |
| Switch | 802.1D bridge: MAC learning, unicast forwarding, flooding, VLANs |
| Layer 3 switch | Bridges within a VLAN and routes between VLAN interfaces (SVIs) |
| Firewall | A router plus an ordered first-match permit/deny policy |
| Access point | Two-port bridge joining a wireless segment to the wired network |
| Hub | Repeats every frame to every other port; no learning, no filtering |
| Cloud | Stands in for an external network behind a routed boundary |

### Protocols

Ethernet II with 802.1Q tagging · ARP · IPv4 · ICMP · UDP · TCP (header codec) ·
DHCP · DNS

### Simulation

- Deterministic, event-driven engine with a clock independent of the frame rate
- Play, pause, step one event at a time, stop, reset, adjustable speed
- Packets animate along the links they are actually travelling
- A structured event stream that the log, the timeline, the tests and learning mode all read

### Analysis

- Packet inspector with a layer-by-layer breakdown, byte offsets, checksum verification and a hex view
- Per-packet path history across every hop
- Validation with 17 rules covering addressing, topology, VLANs, services and routing
- Automated connectivity tests stored in the project, each run on a private copy of it
- Learning mode, which narrates the engine's events in plain language
- An integrated device console: `show ip route`, `show arp`, `show mac address-table`,
  `ping`, and configuration commands that go through the same undo stack as the editor

---

## Screenshots

Not included in the repository. Run `tnp` and open **File → New from sample** to see the
application with a working network in it.

---

## Requirements

- A C++20 compiler — MSVC 19.30+ (Visual Studio 2022), GCC 13+, or Clang 16+
- CMake 3.20 or newer
- A GPU with OpenGL 3.2 support, for the graphical build

On Linux, the graphical build additionally needs the X11 and Wayland development packages
that GLFW compiles against:

```bash
sudo apt-get install -y \
  libgl1-mesa-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev
```

Configure with `-DTNP_BUILD_UI=OFF` (or use the `linux-headless` preset) to build the engine
and `tnpcli` without any of them.

Dependencies are downloaded automatically at configure time and pinned to exact versions:

| Dependency | Version | Used for |
| --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.92.9b (docking) | User interface |
| [GLFW](https://www.glfw.org/) | 3.4 | Window and input |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Project serialization |
| [Catch2](https://github.com/catchorg/Catch2) | 3.8.1 | Tests |

Set `-DTNP_USE_SYSTEM_PACKAGES=ON` to prefer packages already installed on the machine.

---

## Building

```bash
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Presets are provided for `windows-debug`, `windows-release`, `windows-ninja-debug`,
`linux-debug`, `linux-release`, `linux-headless`, `macos-debug` and `macos-release`.

Without presets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Useful options:

| Option | Default | Effect |
| --- | --- | --- |
| `TNP_BUILD_UI` | `ON` | Build the graphical application. `OFF` builds only the engine and the headless tool. |
| `TNP_BUILD_TESTS` | `ON` | Build the test suite. |
| `TNP_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors. CI turns this on. |
| `TNP_USE_SYSTEM_PACKAGES` | `OFF` | Prefer installed packages over downloading. |

The build produces two binaries in `build/<preset>/bin`:

- `tnp` — the application
- `tnpcli` — a headless tool for validating, testing and exporting projects from a script

---

## Testing

```bash
ctest --preset linux-release
```

or directly:

```bash
cd build && ctest --output-on-failure
```

The suite is split by layer so a failure points at the module that broke:

| Executable | Covers |
| --- | --- |
| `tnp_tests_core` | Addresses, subnets, interfaces, devices, the topology graph |
| `tnp_tests_protocols` | Wire encoding and decoding, checksums, malformed input |
| `tnp_tests_routing` | Longest-prefix match, administrative distance, static routes |
| `tnp_tests_simulation` | ARP, MAC learning, forwarding, TTL, firewall, determinism |
| `tnp_tests_serialization` | Round trips, the `.tnp` container, corrupt and hostile files |
| `tnp_tests_validation` | Every validation rule |
| `tnp_tests_integration` | Undo/redo, the console, and the complete workflow |

---

## The headless tool

`tnpcli` drives the same `Application` the window does, which is how the engine is exercised
in CI without a display.

```bash
tnpcli demo network.tnp          # write the sample project
tnpcli info network.tnp          # summarise it
tnpcli validate network.tnp      # run the validation rules
tnpcli test network.tnp          # run the project's connectivity tests
tnpcli ping network.tnp PC1 Server1
tnpcli export network.tnp topology.svg
tnpcli convert network.tnp network.tnpjson
```

`validate` and `test` exit non-zero on failure, so they drop straight into a pipeline.

---

## Project format

TNP saves to **`.tnp`**, a small versioned container: a magic number, a container version,
a table of named entries with CRC-32 checksums, then the data. Today it holds a manifest and
the project document; the format exists so assets and per-section files can be added without
changing what a `.tnp` file *is*.

**`.tnpjson`** is the same project as readable, hand-editable JSON. It is the development and
interchange format, and `tnpcli convert` moves between the two.

**`.tnpenc`** is reserved for encrypted projects and is **not implemented**. TNP will not ship
a hand-rolled cipher; see [`docs/FILE_FORMAT.md`](docs/FILE_FORMAT.md) for the reasoning and
the full schema.

Saving is atomic: the new content is written to a temporary file and only then replaces the
original, so a crash or a full disk during save cannot destroy existing work. Autosave writes
a recovery copy on an interval and deletes it on a clean exit, which is precisely what makes
the next start-up able to tell a crash from a normal close.

---

## Project structure

```text
src/
├── utilities/      identifiers, time, logging, byte streams, file helpers
├── core/           the domain model - knows nothing above it
│   ├── network/    addresses, interfaces, links, devices, the topology graph
│   ├── protocols/  wire formats: Ethernet, ARP, IPv4, ICMP, UDP, TCP, DHCP, DNS
│   ├── routing/    routes, the forwarding table, static routing, OSPF configuration
│   ├── devices/    the IPv4 stack, the switching engine, and the nine device types
│   └── project/    project, metadata, layout, annotations, tests, settings
├── simulation/     the clock, the event queue, packet history, the engine
├── serialization/  the .tnpjson schema and the .tnp container
├── validation/     the rule framework and the built-in rules
├── testing/        the connectivity-test runner
├── commands/       undo/redo
├── cli/            the integrated device console
├── app/            document lifecycle, autosave, recovery, export, learning mode
└── ui/             the Dear ImGui workspace
tests/              one executable per layer
docs/               architecture, file format, roadmap
```

---

## What is and is not simulated

TNP is explicit about its boundaries. The application says so too: **Help → About** lists
this, and the validator raises an informational issue when a project configures something
this build stores but does not run.

**Simulated, with real bytes on the wire**

- Ethernet II framing, minimum-frame padding, 802.1Q tagging
- MAC learning, ageing, unicast forwarding, unknown-unicast and broadcast flooding, per-VLAN separation
- ARP: cache, request and reply, retries, timeout, queueing behind an unresolved address
- IPv4: header encoding, checksum verification, TTL decrement, longest-prefix-match forwarding
- ICMP: echo request and reply, destination unreachable, time exceeded, with the quoted datagram
- DHCP: the four-message allocation exchange, leases, exclusions, and a real client
- DNS: an authoritative zone answering A queries
- Firewall: an ordered first-match policy consulted on the forwarding path

**Not simulated in this build**

| Feature | State |
| --- | --- |
| TCP | Header codec and state enumeration exist; no connection state machine runs. A segment is observed and discarded. |
| OSPF | Configuration is stored, serialized and reported. No adjacencies form and no routes are computed from it. |
| Spanning tree | A physical loop between bridges is **detected and reported** by the validator, but not broken. |
| IPv4 fragmentation | An oversized packet is dropped and reported, never fragmented. |
| NAT | Not implemented. |
| IPv6 forwarding | Addresses are modelled, parsed and serialized; no IPv6 traffic is simulated. |
| Wireless | Association is topological. No contention, signal strength or roaming. |
| Encryption (`.tnpenc`) | Not implemented, deliberately. |
| PNG/PDF export | SVG only. |

---

## Development notes

**Layering is enforced by the build.** Each directory under `src/` is a separate CMake target
and the dependency arrow only points one way:

```text
ui → app → {cli, testing, commands, validation, simulation, serialization} → core → utilities
```

The domain model cannot include a simulator header, and nothing below `ui` can include Dear
ImGui. If a change needs to break that, the design is wrong — see
[`ARCHITECTURE.md`](ARCHITECTURE.md) for how the layers talk to each other instead.

**Every editor change is a command.** Nothing writes to the project model directly except
through `commands/`, which is why undo works everywhere, including from the device console.

**Rules for contributions**

- No placeholder that reports success. If something is not implemented, say so in the code,
  in the validator, and in this README.
- New device kinds are registered in `DeviceRegistry`; nothing else should switch on `DeviceType`.
- New validation rules are separate `ValidationRule` objects, so they can be tested and
  disabled individually.
- Protocol changes come with round-trip tests, including for malformed input.

---

## Roadmap

See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the full plan. In short:

1. **Next** — TCP connection state machine; a DNS resolver client so `ping <hostname>` works
2. **Then** — OSPF adjacencies and SPF; spanning tree; NAT
3. **Later** — PNG and PDF export; `.tnpenc` with a vetted AEAD; a plugin interface for
   custom devices and protocols

---

## Licence

MIT. See [`LICENSE`](LICENSE).
