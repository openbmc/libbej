#include "bej_encoder_json.hpp"

namespace libbej
{

bool stackEmpty(void* dataPtr)
{
    return (reinterpret_cast<std::vector<void*>*>(dataPtr))->empty();
}

void* stackPeek(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    if (stack->empty())
    {
        return nullptr;
    }
    return stack->back();
}

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

int stackPush(void* property, void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    stack->emplace_back(property);
    return 0;
}

int getBejEncodedBuffer(const void* data, size_t dataSize, void* handlerContext)
{
    auto stack = reinterpret_cast<std::vector<uint8_t>*>(handlerContext);
    const uint8_t* dataBuf = reinterpret_cast<const uint8_t*>(data);
    stack->insert(stack->end(), dataBuf, dataBuf + dataSize);
    return 0;
}

std::vector<uint8_t> BejEncoderJson::getOutput()
{
    std::vector<uint8_t> currentEncodedPayload = std::move(encodedPayload);
    // Re-Initialize encodedPayload with empty vector to be used again for
    // next encoding
    encodedPayload = {};

    return currentEncodedPayload;
}

int BejEncoderJson::encode(const struct BejDictionaries* dictionaries,
                           enum BejSchemaClass schemaClass,
                           struct RedfishPropertyParent* root)
{
    struct BejEncoderOutputHandler output = {
        .handlerContext = &encodedPayload,
        .recvOutput = &getBejEncodedBuffer,
    };

    struct BejPointerStackCallback stackCallbacks = {
        .stackContext = &stack,
        .stackEmpty = stackEmpty,
        .stackPeek = stackPeek,
        .stackPop = stackPop,
        .stackPush = stackPush,
        .deleteStack = nullptr,
    };

    return bejEncode(dictionaries, BEJ_DICTIONARY_START_AT_HEAD, schemaClass,
                     root, &output, &stackCallbacks);
}

} // namespace libbej
