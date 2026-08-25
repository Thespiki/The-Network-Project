#include "serialization/TnpContainer.h"

#include "utilities/ByteStream.h"
#include "utilities/Time.h"

#include <algorithm>
#include <array>
#include <format>

namespace tnp::serial {
namespace {

constexpr std::array<u8, 4> kMagic = {'T', 'N', 'P', 'C'};
constexpr std::size_t kHeaderSize = 12;

/// Record size for one entry, excluding the variable-length name.
constexpr std::size_t kFixedRecordSize = 2 /*name length*/ + 1 /*compression*/ + 4 /*length*/ +
                                         4 /*crc*/ + 4 /*offset*/;

/// Refuses absurd values before allocating: a corrupt length field must not turn
/// into a multi-gigabyte allocation.
constexpr u32 kMaxEntrySize = 256u * 1024u * 1024u;
constexpr u32 kMaxEntryCount = 4096;

} // namespace

void TnpContainer::addEntry(std::string name, ByteBuffer data) {
    const auto existing = std::find_if(entries_.begin(), entries_.end(),
                                       [&](const TnpEntry& entry) { return entry.name == name; });
    if (existing != entries_.end()) {
        existing->data = std::move(data);
        return;
    }
    entries_.push_back(TnpEntry{std::move(name), std::move(data), 0});
}

void TnpContainer::addTextEntry(std::string name, std::string_view text) {
    addEntry(std::move(name), ByteBuffer{text.begin(), text.end()});
}

const ByteBuffer* TnpContainer::find(std::string_view name) const {
    const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                    [name](const TnpEntry& candidate) { return candidate.name == name; });
    return entry == entries_.end() ? nullptr : &entry->data;
}

std::optional<std::string> TnpContainer::findText(std::string_view name) const {
    const ByteBuffer* data = find(name);
    if (data == nullptr) return std::nullopt;
    return std::string{data->begin(), data->end()};
}

ByteBuffer TnpContainer::serialize() const {
    // The table records absolute offsets, so the size of the table has to be
    // known before the first byte is written.
    std::size_t tableSize = 0;
    for (const TnpEntry& entry : entries_) tableSize += kFixedRecordSize + entry.name.size();

    std::size_t dataOffset = kHeaderSize + tableSize;

    ByteBuffer bytes;
    ByteWriter writer{bytes};

    writer.bytes(std::span<const u8>{kMagic});
    writer.u16v(kVersion);
    writer.u16v(0); // flags
    writer.u32v(static_cast<u32>(entries_.size()));

    for (const TnpEntry& entry : entries_) {
        writer.u16v(static_cast<u16>(entry.name.size()));
        writer.bytes(std::span<const u8>{reinterpret_cast<const u8*>(entry.name.data()),
                                         entry.name.size()});
        writer.u8v(entry.compression);
        writer.u32v(static_cast<u32>(entry.data.size()));
        writer.u32v(crc32(entry.data));
        writer.u32v(static_cast<u32>(dataOffset));
        dataOffset += entry.data.size();
    }

    for (const TnpEntry& entry : entries_) writer.bytes(entry.data);
    return bytes;
}

bool TnpContainer::looksLikeContainer(const ByteBuffer& bytes) {
    return bytes.size() >= kMagic.size() &&
           std::equal(kMagic.begin(), kMagic.end(), bytes.begin());
}

Result<TnpContainer> TnpContainer::parse(const ByteBuffer& bytes) {
    if (!looksLikeContainer(bytes)) {
        return Result<TnpContainer>::failure("this is not a TNP container (bad magic number)");
    }

    ByteReader reader{bytes};
    if (!reader.skip(kMagic.size())) {
        return Result<TnpContainer>::failure("the file is truncated");
    }

    const auto version = reader.u16v();
    const auto flags = reader.u16v();
    const auto count = reader.u32v();
    if (!version || !flags || !count) {
        return Result<TnpContainer>::failure("the container header is truncated");
    }
    if (*version > kVersion) {
        return Result<TnpContainer>::failure(
            std::format("container version {} was written by a newer build of TNP (this build reads up to {})",
                        *version, kVersion));
    }
    if (*count > kMaxEntryCount) {
        return Result<TnpContainer>::failure(
            std::format("the container claims {} entries, which is beyond the supported limit", *count));
    }

    struct Record {
        std::string name;
        u8 compression = 0;
        u32 length = 0;
        u32 checksum = 0;
        u32 offset = 0;
    };

    std::vector<Record> records;
    records.reserve(*count);

    for (u32 i = 0; i < *count; ++i) {
        const auto nameLength = reader.u16v();
        if (!nameLength) return Result<TnpContainer>::failure("the entry table is truncated");

        const auto nameBytes = reader.bytes(*nameLength);
        if (!nameBytes) return Result<TnpContainer>::failure("the entry table is truncated");

        Record record;
        record.name.assign(reinterpret_cast<const char*>(nameBytes->data()), nameBytes->size());

        const auto compression = reader.u8v();
        const auto length = reader.u32v();
        const auto checksum = reader.u32v();
        const auto offset = reader.u32v();
        if (!compression || !length || !checksum || !offset) {
            return Result<TnpContainer>::failure("the entry table is truncated");
        }
        if (*compression != 0) {
            return Result<TnpContainer>::failure(
                std::format("entry '{}' uses compression method {}, which this build cannot read",
                            record.name, *compression));
        }
        if (*length > kMaxEntrySize) {
            return Result<TnpContainer>::failure(
                std::format("entry '{}' declares an implausible size", record.name));
        }

        record.compression = *compression;
        record.length = *length;
        record.checksum = *checksum;
        record.offset = *offset;
        records.push_back(std::move(record));
    }

    TnpContainer container;
    for (const Record& record : records) {
        const std::size_t end = static_cast<std::size_t>(record.offset) + record.length;
        if (record.offset > bytes.size() || end > bytes.size()) {
            return Result<TnpContainer>::failure(
                std::format("entry '{}' points outside the file", record.name));
        }

        ByteBuffer data(bytes.begin() + record.offset, bytes.begin() + static_cast<std::ptrdiff_t>(end));
        if (crc32(data) != record.checksum) {
            return Result<TnpContainer>::failure(
                std::format("entry '{}' is corrupt (checksum mismatch)", record.name));
        }
        container.entries_.push_back(TnpEntry{record.name, std::move(data), record.compression});
    }

    return container;
}

} // namespace tnp::serial
