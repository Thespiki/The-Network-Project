# Roadmap

What is done, what is next, and why the unfinished parts are unfinished.

---

## Complete

**Milestone 1 — Foundation.** CMake project with presets, Dear ImGui/GLFW application,
domain model, device/interface/link system, canvas, selection, serialization, Catch2 tests,
logging.

**Milestone 2 — Editor.** Device palette, properties panel, interface-level linking, zoom,
pan, grid, snapping, delete, copy/paste, undo/redo, project management.

**Milestone 3 — Basic networking.** MAC addresses, Ethernet with 802.1Q, IPv4, subnet
arithmetic, ARP, ICMP, static routing with longest-prefix match.

**Milestone 4 — Simulation.** Deterministic event-driven engine, packet generation and
forwarding, visual packet movement, play/pause/step/reset, packet inspector.

**Milestone 5 — Diagnostics.** Validation with 17 rules, problems panel, centralised logging,
automated connectivity tests, per-packet path history.

---

## Partly done

**Milestone 6 — Advanced networking.**

| Feature | State |
| --- | --- |
| VLANs | **Done.** Access and trunk ports, tagging on egress, per-VLAN forwarding, SVIs on a layer-3 switch. |
| DHCP | **Done.** Server with pools, exclusions and leases; a real client that runs the four-message exchange. |
| DNS | **Done.** Authoritative zone answering A queries over the real wire format. |
| Firewall | **Done.** Ordered first-match policy on the forwarding path, with hit counts. |
| UDP | **Done.** Full datagram encoding with pseudo-header checksums; carries DHCP and DNS. |
| TCP | **Partial.** Header codec, flags and the state enumeration exist. No connection state machine runs. |
| NAT | **Not started.** |
| OSPF | **Configuration only.** Stored, serialized and reported; no adjacencies, no SPF. |

**Milestone 7 — Professional features.**

| Feature | State |
| --- | --- |
| Device console | **Done.** Show commands, ping, and configuration through the undo stack. |
| Export | **SVG done.** PNG and PDF not started. |
| Autosave and recovery | **Done.** |
| Learning mode | **Done.** |
| `.tnp` container | **Done.** |
| `.tnpenc` | **Not started, deliberately.** |

---

## Next

### 1. TCP

The header codec and `TcpState` exist; what is missing is the state machine and a socket
abstraction. The plan:

- A `TcpConnection` component owned by `Ipv4Stack`, keyed by the four-tuple
- The RFC 9293 state machine driven by segment arrival and timers, both already available
  through `DeviceContext`
- Retransmission with a fixed timeout first; congestion control is out of scope for a teaching
  simulator until there is a reason for it
- A listening service on `Server`, so an HTTP-shaped exchange can be watched end to end

The packet pipeline needs no change: a TCP segment is already decoded and inspected today.

### 2. A DNS resolver client

`ping <hostname>` is currently refused rather than faked. A resolver on `Ipv4Stack` that sends
a real query to the configured server and waits for the answer would make it work — and make
the DNS exchange visible in the inspector, which is the interesting part.

### 3. OSPF

The routing table already supports `RouteSource::Ospf` with its own administrative distance,
so this is additive:

- Hello packets and the neighbour state machine, over the existing timer mechanism
- LSA flooding within a single area
- Dijkstra over the link-state database, installing routes with source `Ospf`
- Multi-area support after that

Until then the validator says so, in the application, with an informational issue.

### 4. Spanning tree

TNP already **detects** a physical loop between bridges and reports it, because a loop with
no STP is a broadcast storm and the user should know. Breaking it needs RSTP: bridge
priorities, port roles and states, and topology change handling.

---

## Later

- **NAT**, on the firewall and the router: static, dynamic, and overload
- **PNG and PDF export** — the SVG renderer already writes from the project model rather than
  from the canvas, so both are a back end away
- **IPv6 forwarding.** Addresses, prefixes and interface configuration exist and round-trip;
  what is missing is the forwarding path, neighbour discovery and ICMPv6
- **`.tnpenc`**, once a vetted AEAD and a memory-hard KDF can be depended on. See
  [`FILE_FORMAT.md`](FILE_FORMAT.md) for why this is not being rushed
- **A plugin interface.** `DeviceRegistry` is already an ordinary object with a factory table
  rather than a switch statement, so a plugin registering a device kind needs a loading
  mechanism and a stable ABI, not a redesign
- **Import** from GraphML and other topology formats
- **Traceroute** in the console, which the TTL and ICMP time-exceeded handling already support

---

## Deliberately not planned

- **A full production TCP/IP stack.** TNP simulates enough to teach and to diagnose. Window
  scaling, SACK and modern congestion control would add complexity without adding insight.
- **Bit-level physical simulation.** Collisions on a hub are modelled as repetition, not as
  contention. Signal strength and interference on wireless are not modelled at all.
- **Hand-rolled cryptography**, under any circumstances.
