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
     * @param[in] majorSchemaStartingOffset - dictionary starting offset of the
     * major schema. Usually this will be set to BEJ_DICTIONARY_START_AT_HEAD
     * unless we are decoding a payload associated with a bejLocator type.
     * @return 0 if successful.
     */
    int decode(const BejDictionaries& dictionaries,
               const std::span<const uint8_t> encodedPldmBlock,
               uint16_t majorSchemaStartingOffset = BEJ_DICTIONARY_START_AT_HEAD);

    /**
     * @brief Get the JSON output related to the latest call to decode.
     *
     * @return std::string containing a JSON. If the decoding was
     * unsuccessful, this might contain partial data (invalid JSON).
     */
    std::string getOutput();

  private:
    bool isPrevAnnotated;
    std::string output;
    std::vector<BejStackProperty> stack;
};

} // namespace libbej
