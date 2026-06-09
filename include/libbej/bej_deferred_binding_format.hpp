#pragma once

#include "bej_deferred_binding.hpp"

#include <cstdint>
#include <format>
#include <string>

namespace libbej
{

/**
 * @brief Builders for BejDeferredBindingMap keys: the placeholder body, with no
 * leading '%'. Both the caller populating the map and libbej formatting
 * placeholders go through these, so the DSP0218 Table 42 token grammar lives in
 * one place.
 */
namespace bejBinding
{

/**
 * @brief Token for the %L<id> resource-URI marker.
 *
 * @param[in] resourceId - 32-bit resource id.
 * @return the token, e.g. "L30".
 */
inline std::string resourceLink(uint32_t resourceId)
{
    return std::format("{}{}", bejDeferredBindingResourceLink, resourceId);
}

/**
 * @brief Token for the %I<id> instance-id marker.
 *
 * @param[in] resourceId - resource id.
 * @return the token, e.g. "I30".
 */
inline std::string instanceId(uint32_t resourceId)
{
    return std::format("{}{}", bejDeferredBindingInstanceId, resourceId);
}

/**
 * @brief Token for the %T<id>.<action> action-target marker.
 *
 * @param[in] resourceId - resource id.
 * @param[in] actionId - action id (widened so it renders as a number).
 * @return the token, e.g. "T30.1".
 */
inline std::string action(uint32_t resourceId, uint8_t actionId)
{
    return std::format("{}{}{}{}", bejDeferredBindingActionUri, resourceId,
                       bejDeferredBindingActionSeparator,
                       static_cast<uint32_t>(actionId));
}

/**
 * @brief Token for the id-less %C containing-chassis marker.
 *
 * @return the token "C".
 */
inline std::string chassis()
{
    return std::string(1, bejDeferredBindingChassis);
}

/**
 * @brief Token for the id-less %S containing-system marker.
 *
 * @return the token "S".
 */
inline std::string system()
{
    return std::string(1, bejDeferredBindingSystem);
}

} // namespace bejBinding

} // namespace libbej
