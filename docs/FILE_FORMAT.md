# TNP project formats

TNP writes three file types. Two are implemented; one is reserved.

| Extension | What it is | State |
| --- | --- | --- |
| `.tnp` | The project container | Implemented |
| `.tnpjson` | The same project as readable JSON | Implemented |
| `.tnpenc` | An encrypted container | **Not implemented** |

The format is decided by content where possible — the container starts with a magic number —
and by extension otherwise, so a `.tnp` file that has been renamed still opens.

---

## Versioning

The project document carries a `major.minor` version, currently **1.0**.

- A **newer major** version is refused. Its structure may have changed in ways this build
  cannot interpret, and guessing would corrupt the user's work.
- A **newer minor** version loads, with a warning. Minor bumps only add optional fields; an
  older build reads what it understands and ignores the rest. Saving afterwards drops what it
  did not understand, and the warning says so.

The container has its own version, independent of the document's.

---

## `.tnpjson`

A single JSON object. It is the development and interchange format: readable, diffable, and
safe to hand-edit.

```json
{
  "tnp": { "format": "project", "version": "1.0" },
  "metadata": { },
  "simulation": { },
  "network": { "devices": [], "links": [] },
  "layout": { },
  "annotations": [],
  "tests": []
}
```

Reading is forgiving by design. A malformed field costs that field and produces a warning; it
does not fail the load. Only structural problems — invalid JSON, a document that is not a
project, an unreadable major version — refuse the file, and they do so *before* the current
project is touched.

### `tnp`

| Field | Type | Notes |
| --- | --- | --- |
| `format` | string | Must be `"project"`. |
| `version` | string | `"major.minor"`. |

### `metadata`

| Field | Type | Notes |
| --- | --- | --- |
| `id` | uuid | Stable project identifier. |
| `name`, `description`, `author` | string | |
| `tags` | string[] | |
| `createdAt`, `modifiedAt` | string | ISO-8601 UTC. |
| `writtenBy` | string | The build that last wrote the file, for diagnostics. |

### `simulation`

| Field | Type | Default |
| --- | --- | --- |
| `speedMultiplier` | number | `1.0` |
| `maximumStepPerFrameNs` | integer | `100000000` |
| `traceHistoryLimit` | integer | `20000` |
| `packetHistoryLimit` | integer | `2000` |
| `learningMode` | boolean | `false` |
| `autoStartOnTraffic` | boolean | `true` |

### `network.devices[]`

| Field | Type | Notes |
| --- | --- | --- |
| `id` | uuid | Preserved across save and load. An invalid one produces a new identifier and a warning. |
| `type` | string | `Pc`, `Server`, `Router`, `Switch`, `Layer3Switch`, `Firewall`, `AccessPoint`, `Hub`, `Cloud`. An unknown type skips the device with an error. |
| `name` | string | |
| `description` | string | Omitted when empty. |
| `interfaces` | array | See below. Replaces the defaults entirely. |
| `ipv4` | object | Present when the device has an IP stack. |
| `switching` | object | Present when the device bridges. |
| `firewall` | object | Firewalls only. |
| `dhcpServer`, `dnsServer` | object | Devices offering the service. |

#### `interfaces[]`

| Field | Type | Notes |
| --- | --- | --- |
| `id` | uuid | Referenced by links, static routes and OSPF settings. |
| `name` | string | e.g. `GigabitEthernet0/1`. |
| `type` | string | `Ethernet`, `FastEthernet`, `GigabitEthernet`, `TenGigabitEthernet`, `Serial`, `Wireless`, `Loopback`, `Vlan`, `Console`. |
| `mac` | string | `AA:BB:CC:DD:EE:FF`. |
| `adminState` | string | `up` or `down`. |
| `mtu` | integer | 68–9216. |
| `speedMbps` | integer | |
| `duplex` | string | `auto`, `half`, `full`. |
| `dhcp` | boolean | The address is requested at run time. |
| `ipv4` | string[] | CIDR, e.g. `"192.168.1.10/24"`. **Omitted when `dhcp` is true.** |
| `ipv6` | string[] | CIDR. |
| `vlan` | object | `mode` (`access`/`trunk`), `accessVlan`, `nativeVlan`, `allowed` (integer array). |
| `displayName`, `description` | string | Omitted when empty. |

A DHCP interface stores no addresses on purpose: whatever it holds during a run was *leased*,
and writing it would turn a lease into a permanent setting the next time the project opened.

#### `ipv4`

| Field | Type | Notes |
| --- | --- | --- |
| `forwarding` | boolean | What makes a router a router. |
| `staticRoutes` | array | `id`, `destination` (CIDR), `nextHop`, `interface`, `metric`, `enabled`, `description`. |
| `dnsServers` | string[] | |
| `domainName` | string | |
| `ospf` | object | `enabled`, `processId`, `routerId`, `networks[]`, `interfaces[]`, `redistributeConnected`, `redistributeStatic`. Stored and reported; **not simulated**. |

Only *configured* static routes are stored. Connected routes are derived from interface state
whenever it changes and are never written: a saved connected route goes stale the moment an
interface is renumbered.

#### `switching`

`learning` (boolean), `ageingSeconds` (integer), `vlans[]` (`id`, `name`). VLAN 1 always
exists and is reinstated if a file omits it.

#### `firewall`

`defaultAction` (`permit`/`deny`) and `rules[]`: `id`, `name`, `action`, `protocol`
(`ip`/`icmp`/`tcp`/`udp`), `source`, `destination` (CIDR; absent means any), `portFirst`,
`portLast`, `enabled`, `description`. Rules are evaluated in order and the first match decides.

Hit counts are runtime state and are not serialized.

#### `dhcpServer` and `dnsServer`

`dhcpServer`: `enabled` plus `pools[]` — `id`, `name`, `subnet`, `rangeFirst`, `rangeLast`,
`gateway`, `dns`, `domainName`, `leaseSeconds`, `exclusions[]`.

`dnsServer`: `enabled` plus `records[]` — `id`, `name`, `address`, `ttl`.

Leases are runtime state and are not serialized.

### `network.links[]`

| Field | Type | Notes |
| --- | --- | --- |
| `id` | uuid | |
| `a`, `b` | object | `{ "device": uuid, "interface": uuid }`. |
| `medium` | string | `copper`, `fiber`, `serial`, `wireless`, `virtual`. |
| `propagationDelayNs` | integer | One-way delay. Serialized so timings reproduce on any machine. |
| `bandwidthMbps` | integer | |
| `enabled` | boolean | A disabled link stays in the project and carries nothing — how a cable fault is modelled. |
| `label` | string | Shown on the canvas. |

A link whose interfaces cannot be found is dropped with a warning rather than leaving a
dangling reference.

### `layout`

```json
{
  "devices": [ { "device": "uuid", "x": -380, "y": 0, "locked": false } ],
  "view":    { "x": 0, "y": 0, "zoom": 1.0 },
  "grid":    { "visible": true, "snap": false, "size": 24 }
}
```

Sorted by device identifier so the file is stable across saves. Entries for devices that no
longer exist are pruned on load.

### `annotations[]`

`id`, `kind` (`Text`, `Rectangle`, `Ellipse`, `Arrow`, `NetworkLabel`), `start`, `end`
(`{x, y}`), `text`, `color`, `fillColor` (packed `0xAABBGGRR`), `fontSize`, `thickness`,
`filled`, `zOrder`.

### `tests[]`

`id`, `name`, `description`, `source` (device uuid), `destinationDevice` **or**
`destinationAddress`, `protocol`, `expectation` (`reachable`/`unreachable`), `probeCount`,
`timeoutNs`, `payloadSize`, `enabled`.

A test naming a device that no longer exists is pruned on load.

---

## `.tnp` container

A small archive, explicitly versioned. All integers are **big-endian**.

```text
offset  size  field
0       4     magic, "TNPC"
4       2     container version (currently 1)
6       2     flags (reserved, zero)
8       4     entry count
12      ..    entry table, then the data blocks
```

Each entry-table record:

```text
size  field
2     name length
n     name (UTF-8)
1     compression method (0 = stored; no codec is implemented yet)
4     stored length
4     CRC-32 of the stored bytes (IEEE 802.3 polynomial)
4     absolute offset of the data
```

Entries written today:

| Name | Contents |
| --- | --- |
| `manifest.json` | Describes the container: format, versions, project name, timestamp, entry list. |
| `project.tnpjson` | The complete project document, compact. |

The container exists so a project can grow — assets, per-section files, capture data —
without changing what a `.tnp` file *is*. The compression field is reserved so adding a codec
later does not need a new container version.

**On reading**, the magic number, the version and every entry's CRC-32 are verified, and
implausible lengths are rejected before anything is allocated. A file that fails any check is
reported, never partially loaded.

---

## `.tnpenc` — not implemented

The extension is reserved and the interface exists (`serialization/TnpCrypto.h`), but calling
it returns a clear failure. It is not a stub that pretends to work.

The reason is deliberate. Doing this properly needs a vetted authenticated-encryption
implementation — AES-256-GCM or XChaCha20-Poly1305 — and a memory-hard password KDF such as
Argon2id or scrypt. TNP has no cryptographic dependency today, and inventing one, or shipping
something that merely looks encrypted, would be **worse than shipping nothing**: a user would
believe a project is protected when it is not.

The `.tnp` format also has to settle first. Encrypting a container whose layout is still
moving would produce files no future build could open.

When it lands, encryption will wrap a finished container and know nothing about the project
model — which is why the boundary is already drawn where it is.

---

## Writing files safely

Every save goes to a temporary file next to the destination and only then replaces the
original with an atomic rename. A crash, a full disk or a power loss during a save cannot
destroy work that was already on disk.

Autosave writes a recovery copy to the user state directory on an interval, together with a
marker recording where the work came from. Both are deleted when the project is saved and
when the application closes normally. A recovery copy present at start-up therefore means
exactly one thing: the previous session did not close cleanly.
