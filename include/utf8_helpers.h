#ifndef UTF8_HELPERS_H
#define UTF8_HELPERS_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace turndown_cpp::utf8 {

// UTF-8 decoding masks and boundaries:
// - kAsciiMask identifies ASCII bytes (leading bit clear).
// - kLeadPayloadMask picks out payload bits from a lead byte once the length is known.
// - kMask2/3/4 and kLead2/3/4 identify 2/3/4-byte lead-byte patterns.
// - kContinuationMask/Sig/Payload validate continuation bytes and extract their payload bits.
// - kMinValues detects overlong encodings; surrogate/Unicode bounds reject invalid ranges.
constexpr unsigned char kAsciiMask        = 0x80; // 1000 0000
constexpr unsigned char kLeadPayloadMask  = 0x7F; // 0111 1111
constexpr unsigned char kMask2            = 0xE0; // 1110 0000
constexpr unsigned char kMask3            = 0xF0; // 1111 0000
constexpr unsigned char kMask4            = 0xF8; // 1111 1000
constexpr unsigned char kLead2            = 0xC0; // 1100 0000
constexpr unsigned char kLead3            = 0xE0; // 1110 0000
constexpr unsigned char kLead4            = 0xF0; // 1111 0000
constexpr unsigned char kContinuationMask = 0xC0; // 1100 0000
constexpr unsigned char kContinuationSig  = 0x80; // 1000 0000
constexpr unsigned char kContinuationPayload = 0x3F; // 0011 1111
constexpr std::array<std::uint32_t, 5> kMinValues = {0, 0, 0x80, 0x800, 0x10000}; // min code point per length
constexpr std::uint32_t kSurrogateStart   = 0xD800;
constexpr std::uint32_t kSurrogateEnd     = 0xDFFF;
constexpr std::uint32_t kUnicodeMax       = 0x10FFFF;

// True when the byte is plain ASCII (no multi-byte prefix).
constexpr bool isAsciiLead(unsigned char lead) {
    return (lead & kAsciiMask) == 0;
}

// Infers expected UTF-8 sequence length from the lead-byte pattern.
constexpr std::size_t expectedLength(unsigned char lead) {
    if ((lead & kMask2) == kLead2) return 2;
    if ((lead & kMask3) == kLead3) return 3;
    if ((lead & kMask4) == kLead4) return 4;
    return 1;
}

// Checks whether a byte has the 10xxxxxx continuation signature.
constexpr bool isContinuation(unsigned char byte) {
    return (byte & kContinuationMask) == kContinuationSig;
}

// Validates decoded code points against overlong, surrogate, and range rules.
constexpr bool isInvalidCodepoint(std::uint32_t cp, std::size_t expected_len) {
    bool overlong = cp < kMinValues[expected_len];
    bool surrogate = cp >= kSurrogateStart && cp <= kSurrogateEnd;
    bool too_large = cp > kUnicodeMax;
    return overlong || surrogate || too_large;
}

// Represents a decoded UTF-8 character with its original byte span.
struct Utf8Char {
    std::uint32_t codepoint;
    std::size_t start;
    std::size_t length;
};

// Represents a decoded UTF-8 slice used by whitespace utilities.
struct CodepointSlice {
    std::uint32_t codepoint;
    std::size_t start;
    std::size_t length;
};

} // namespace turndown_cpp::utf8

#endif // UTF8_HELPERS_H

