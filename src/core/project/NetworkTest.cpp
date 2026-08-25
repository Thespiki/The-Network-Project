#include "core/project/NetworkTest.h"

namespace tnp::core {

std::string_view testProtocolName(TestProtocol protocol) {
    switch (protocol) {
        case TestProtocol::Icmp: return "ICMP";
    }
    return "ICMP";
}

std::string_view testExpectationName(TestExpectation expectation) {
    switch (expectation) {
        case TestExpectation::Reachable:   return "reachable";
        case TestExpectation::Unreachable: return "unreachable";
    }
    return "reachable";
}

} // namespace tnp::core
