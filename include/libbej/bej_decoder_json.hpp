#pragma once

#include "bej_common.h"
#include "bej_decoder_core.h"

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
     * @return 0 if successful.
     */
    int decode(const BejDictionaries& dictionaries,
               const std::span<const uint8_t> encodedPldmBlock);

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
     * Defaults to bejTrailingError so existing callers see no
     * behavior change. Use bejTrailingWarn for diagnostics or
     * bejTrailingIgnore to accept over-sized buffers.
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
    BejTrailingDataPolicy trailingPolicy = bejTrailingError;
};

} // namespace libbej
