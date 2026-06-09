#pragma once

#include "bej_common.h"
#include "bej_decoder_core.h"

#include "bej_deferred_binding.hpp"

#include <span>
#include <string>
#include <vector>

namespace libbej
{

/**
 * @brief Class for decoding RDE BEJ to a JSON output.
 */
class BejDecoderJson
{
  public:
    /**
     * @brief Decode the encoded PLDM block.
     *
     * @param[in] dictionaries - dictionaries needed for decoding.
     * @param[in] encodedPldmBlock - encoded PLDM block.
     * @param[in] bindings - resource-id to substitution map selecting one of
     * two modes. Empty (the default) disables resolution: deferred binding
     * placeholders and bejResourceLink ids are left raw (e.g. %L<id>),
     * preserving the input. Non-empty enables resolution: a recognized id is
     * substituted, and an unrecognized one becomes its DSP0218 Table 42 invalid
     * form (%L -> /invalid.PDR<id>, %I -> invalid, %T -> /invalid.<id>.<n>).
     * @return 0 if successful.
     */
    int decode(const BejDictionaries& dictionaries,
               const std::span<const uint8_t> encodedPldmBlock,
               const BejDeferredBindingMap& bindings = {});

    /**
     * @brief Get the JSON output related to the latest call to decode.
     *
     * @return std::string containing a JSON. If the decoding was
     * unsuccessful, this might contain partial data (invalid JSON).
     */
    std::string getOutput();

    /**
     * @brief Choose how the decoder reacts to buffer bytes that lie
     * past the encoded payload (i.e. past the root SFLV's value
     * length).
     *
     * Defaults to bejTrailingIgnore, which accepts over-sized buffers
     * (e.g. fixed-size PLDM messages). Use bejTrailingWarn for
     * diagnostics or bejTrailingError to reject malformed input.
     *
     * @param[in] policy - trailing-data policy to apply on subsequent
     * decode() calls.
     */
    void setTrailingDataPolicy(BejTrailingDataPolicy policy)
    {
        trailingPolicy = policy;
    }

  private:
    bool isPrevAnnotated;
    std::string output;
    std::vector<BejStackProperty> stack;
    BejTrailingDataPolicy trailingPolicy = bejTrailingIgnore;
};

} // namespace libbej
