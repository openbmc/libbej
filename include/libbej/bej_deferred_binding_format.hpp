#pragma once

#include "bej_deferred_binding.hpp"

#include <cstdint>
#include <format>
#include <string>

namespace libbej
{

/**
 * @brief Build a "<marker><subType><resourceId>" deferred binding placeholder.
 *
 * @param[in] subType - substitution character (e.g.
 * bejDeferredBindingResourceLink).
 * @param[in] resourceId - resource identifier (accepts the decoder's 64-bit
 * resource-link id without truncation).
 * @return the placeholder string.
 */
inline std::string bejFormatDeferredBinding(char subType, uint64_t resourceId)
{
    return std::format("{}{}{}", bejDeferredBindingMarker, subType, resourceId);
}

/**
 * @brief Build a "%T<resourceId>.<actionId>" action deferred binding
 * placeholder.
 *
 * @param[in] resourceId - resource identifier.
 * @param[in] actionId - action identifier.
 * @return the placeholder string.
 * @note actionId is widened so std::format renders it as a number, not a
 * character.
 */
inline std::string bejFormatDeferredBindingAction(uint32_t resourceId,
                                                  uint8_t actionId)
{
    return std::format("{}{}{}{}{}", bejDeferredBindingMarker,
                       bejDeferredBindingActionUri, resourceId,
                       bejDeferredBindingActionSeparator,
                       static_cast<uint32_t>(actionId));
}

} // namespace libbej
