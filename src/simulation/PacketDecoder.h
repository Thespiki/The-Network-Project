#pragma once

#include "core/network/Frame.h"
#include "utilities/Types.h"

#include <string>
#include <vector>

namespace tnp::sim {

/// One named value inside a protocol header.
struct DecodedField {
    std::string name;
    std::string value;
    /// Extra explanation, e.g. "correct" next to a checksum.
    std::string detail;
    /// Byte range in the frame, so the inspector can highlight it in the hex view.
    std::size_t offset = 0;
    std::size_t length = 0;
};

/// One protocol header, in encapsulation order.
struct DecodedLayer {
    std::string name;     ///< "Ethernet II", "IPv4", "ICMP"
    std::string summary;  ///< one-line description of this layer
    std::vector<DecodedField> fields;
    std::size_t offset = 0;
    std::size_t length = 0;
};

/// A frame taken apart for display.
struct DecodedPacket {
    std::vector<DecodedLayer> layers;
    /// Highest-layer description, e.g. "Echo (ping) request id=1 seq=3".
    std::string summary;
    /// Set when decoding stopped early, with the reason.
    std::string problem;
    std::size_t totalLength = 0;
};

/// Decodes real wire bytes into a layered view.
///
/// The inspector shows what the decoder finds in the buffer - including a
/// checksum that does not verify - rather than replaying values the sender
/// intended. That is the difference between a packet analyser and a log viewer.
[[nodiscard]] DecodedPacket decodePacket(const ByteBuffer& bytes);

/// Short one-line description used in packet lists and on the canvas.
[[nodiscard]] std::string describeFrame(const ByteBuffer& bytes);

/// Protocol classification of an encoded frame, used when a device builds one.
[[nodiscard]] core::FrameCategory classifyFrame(const ByteBuffer& bytes);

} // namespace tnp::sim
