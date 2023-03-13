#pragma once

#include "bej_common.h"
#include "bej_encoder_core.h"

#include <span>
#include <string>
#include <vector>

namespace libbej
{

/**
 * @brief Class for encoding JSON input.
 */
class BejEncoderJson
{
  public:
    /**
     * @brief Encode the resource data.
     *
     * @param[in] dictionaries - dictionaries needed for encoding.
     * @param[in] schemaClass - BEJ schema class.
     * @param[in] root - pointer to a RedfishPropertyParent struct.
     * @return 0 if successful.
     */
    int encode(const struct BejDictionaries* dictionaries,
               enum BejSchemaClass schemaClass,
               struct RedfishPropertyParent* root);

    /**
     * @brief Get the JSON encoded payload.
     *
     * @return std::vector<uint8_t> containing encoded JSON bytes. If the
     * encoding was unsuccessful, the vector will be empty. Note that the
     * vector resource will be moved to the requester API
     */
    std::vector<uint8_t> getOutput();

  private:
    std::vector<uint8_t> encodedPayload;
    std::vector<void*> stack;
};

} // namespace libbej
