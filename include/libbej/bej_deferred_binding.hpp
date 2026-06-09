#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace libbej
{

// Deferred binding (DSP0218 Table 42) substitution markers. Two transports
// carry substitutions:
//  1. String-valued substitutions travel inside bejString values as
//     "<marker><subType><id>" placeholder text (e.g. %L30), gated by the SFLV
//     deferredBinding format bit and resolved during decode.
//  2. bejResourceLink travels as a numeric resource id (no placeholder text)
//     and is resolved directly from that id.
inline constexpr char bejDeferredBindingMarker = '%';
inline constexpr char bejDeferredBindingResourceLink = 'L'; // %L<id>
inline constexpr char bejDeferredBindingInstanceId = 'I';   // %I<id>
inline constexpr char bejDeferredBindingActionUri = 'T';    // %T<id>.<action>
inline constexpr char bejDeferredBindingActionSeparator =
    '.';                                                    // %T<id>.<action>

/**
 * @brief Deferred binding substitution values for a single resource.
 *
 * Each field holds the value substituted for the corresponding placeholder. On
 * a value collision (the same string reachable from more than one field), the
 * encoder's reverse mapping favours fields in declaration order: uri,
 * instanceId, actionUris, then otherParameters.
 *
 * @note otherParameters must not be keyed by 'L'/'I'/'T'; those have dedicated
 * fields and the decoder resolves them only from those fields.
 */
struct BejDeferredBindingResource
{
    std::string uri;        ///< Resolved URI; substituted for %L<id>.
    std::string instanceId; ///< Resolved instance id; substituted for %I<id>.
    /// Action id -> resolved URI; substituted for %T<id>.<action>.
    std::map<uint8_t, std::string> actionUris;
    /// Substitution character -> value, for Table 42 entries that have no
    /// dedicated field above.
    std::map<char, std::string> otherParameters;
};

/**
 * @brief Deferred binding resources keyed by resource id.
 */
using BejDeferredBindingMap =
    std::unordered_map<uint32_t, BejDeferredBindingResource>;

} // namespace libbej
