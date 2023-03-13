#include "bej_encoder_json.hpp"

namespace libbej
{

/**
 * @brief Callback for stackEmpty. Check if stack is empty
 *
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return true if the stack is empty.
 */

bool stackEmpty(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    return stack->empty();
}

/**
 * @brief Callback for stackPeek. Read the first element from the stack
 *
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return the value of first element in the stack
 */

void* stackPeek(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    if (stack->empty())
    {
        return nullptr;
    }
    return stack->back();
}

/**
 * @brief Callback for stackPop. Remove the top element from the stack.
 *
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return the value of first element in the stack
 */

void* stackPop(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    if (stack->empty())
    {
        return nullptr;
    }
    void* value = stack->back();
    stack->pop_back();
    return value;
}

/**
 * @brief Callback for stackPush. Push a new element to the top of the stack.
 *
 * @param[in] property - property to push.
 * @param[in] dataPtr - pointer to a valid stack of type std::vector<void*>
 * @return 0 if successful.
 */

int stackPush(void* property, void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    stack->push_back(property);
    return 0;
}

/**
 * @brief Callback to get the encoded json payload.
 *
 * @param[in] data - pointer to a valid stack of type std::vector<void*>
 * @param[in] data_size - size of the stack
 * @param[in] handlerContext - Buffer to store the payload
 * @return 0 if successful.
 */

int get_bej_encoded_buffer(const void* data, size_t data_size,
                           void* handlerContext)
{
    auto stack = reinterpret_cast<std::vector<uint8_t>*>(handlerContext);
    const uint8_t* dataBuf = reinterpret_cast<const uint8_t*>(data);
    stack->insert(stack->end(), dataBuf, dataBuf + data_size);
    return 0;
}

std::vector<uint8_t> BejEncoderJson::getOutput()
{
    return std::move(encodedPayload);
}

int BejEncoderJson::encode(const struct BejDictionaries* dictionaries,
                           enum BejSchemaClass schemaClass,
                           struct RedfishPropertyParent* root)
{
    struct BejEncoderOutputHandler output = {
        .handlerContext = &encodedPayload,
        .recvOutput = &get_bej_encoded_buffer,
    };

    struct BejPointerStackCallback stackCallbacks = {
        .stackContext = &stack,
        .stackEmpty = stackEmpty,
        .stackPeek = stackPeek,
        .stackPop = stackPop,
        .stackPush = stackPush,
        .deleteStack = NULL,
    };

    return bejEncode(dictionaries, BEJ_DICTIONARY_START_AT_HEAD, schemaClass,
                     root, &output, &stackCallbacks);
}

} // namespace libbej
