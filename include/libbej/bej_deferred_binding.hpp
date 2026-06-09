#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace libbej
{

// Deferred binding substitution markers (DSP0218 Table 42). The leading '%'
// flags a substitution; the following character selects the marker.
inline constexpr char bejDeferredBindingMarker = '%';
inline constexpr char bejDeferredBindingResourceLink = 'L'; // %L<id>
inline constexpr char bejDeferredBindingInstanceId = 'I';   // %I<id>
inline constexpr char bejDeferredBindingActionUri = 'T';    // %T<id>.<action>
inline constexpr char bejDeferredBindingActionSeparator =
    '.';                                                    // %T<id>.<action>
inline constexpr char bejDeferredBindingChassis = 'C';      // %C
inline constexpr char bejDeferredBindingSystem = 'S';       // %S

/**
 * @brief Deferred binding substitutions, keyed by placeholder body.
 *
 * The key is the placeholder text following the '%' marker, with no leading
 * '%' (the "token"); the value is its substitution. The decoder resolves a
 * placeholder by looking up its token; the encoder rewrites a matching value
 * back to '%' + token. One flat container holds id-bearing markers
 * (%L<id>, %I<id>, %T<id>.<action>) and id-less markers (%C, %S) alike, with no
 * per-resource struct and no empty unused fields.
 *
 *   token "L30"   <-> placeholder "%L30"   (resource URI)
 *   token "I30"   <-> placeholder "%I30"   (instance id)
 *   token "T30.1" <-> placeholder "%T30.1" (action target)
 *   token "C"     <-> placeholder "%C"     (containing chassis link)
 *
 * Build keys with the bejBinding helpers in bej_deferred_binding_format.hpp so
 * the token grammar has a single source of truth.
 */
using BejDeferredBindingMap = std::unordered_map<std::string, std::string>;

} // namespace libbej
