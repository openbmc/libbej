#pragma once

#include "bej_common.h"
#include "bej_encoder_core.h"

#include <vector>

namespace libbej
{

/**
 * @brief Callback for stackEmpty. Check if stack is empty
 *
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return true if the stack is empty.
 */

bool stackEmpty(void* dataPtr);

/**
 * @brief Callback for stackPeek. Read the first element from the stack
 *
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return the value of first element in the stack
 */

void* stackPeek(void* dataPtr);

/**
 * @brief Callback for stackPop. Remove the top element from the stack.
 *
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return the value of first element in the stack
 */

void* stackPop(void* dataPtr);

/**
 * @brief Callback for stackPush. Push a new element to the top of the stack.
 *
 * @param[in] property - property to push.
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return 0 if successful.
 */

int stackPush(void* property, void* dataPtr);

/**
 * @brief Callback to get the encoded json payload.
 *
 * @param[in] data - pointer to a valid stack of type std::vector<void*>
 * @param[in] dataSize - size of the stack
 * @param[in] handlerContext - Buffer to store the payload
 * @return 0 if successful.
 */

int getBejEncodedBuffer(const void* data, size_t dataSize,
                        void* handlerContext);

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
