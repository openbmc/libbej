#include "bej_decoder_json.hpp"

namespace libbej
{

/**
 * @brief This structure is used to pass additional data to callback functions.
 */
struct BejJsonParam
{
    bool* isPrevAnnotated;
    std::string* output;
};

/**
 * @brief Add a property name to output buffer.
 *
 * @param[in] params - a valid BejJsonParam struct.
 * @param[in] propertyName - a NULL terminated string.
 */
static void addPropertyNameToOutput(struct BejJsonParam* params,
                                    const char* propertyName)
{
    if (propertyName[0] == '\0')
    {
        return;
    }
    if (!(*params->isPrevAnnotated))
    {
        params->output->append("\"");
    }
    params->output->append(propertyName);
    params->output->append("\":");
}

/**
 * @brief Callback for bejSet start.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackSetStart(const char* propertyName, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append("{");
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejSet end.
 *
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackSetEnd(void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    params->output->append("}");
    return 0;
}

/**
 * @brief Callback for bejArray start.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackArrayStart(const char* propertyName, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append("[");
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejArray end.
 *
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackArrayEnd(void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    params->output->append("]");
    return 0;
}

/**
 * @brief Callback when an end of a property is detected.
 *
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackPropertyEnd(void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    // Not a section ending. So add a comma.
    params->output->append(",");
    return 0;
}

/**
 * @brief Callback for bejNull type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackNull(const char* propertyName, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append("null");
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejInteger type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - integer value.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackInteger(const char* propertyName, int64_t value, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append(std::to_string(value));
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejEnum type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - a NULL terminated string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackEnum(const char* propertyName, const char* value, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append("\"");
    params->output->append(value);
    params->output->append("\"");
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejString type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - a NULL terminated string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackString(const char* propertyName, const char* value, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append("\"");
    params->output->append(value);
    params->output->append("\"");
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejReal type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - pointing to a valid BejReal.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackReal(const char* propertyName, const struct BejReal* value,
                 void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);

    addPropertyNameToOutput(params, propertyName);
    params->output->append(std::to_string(value->whole));
    params->output->append(".");
    params->output->insert(params->output->cend(), value->zeroCount, '0');
    params->output->append(std::to_string(value->fract));
    if (value->expLen != 0)
    {
        params->output->append("e");
        params->output->append(std::to_string(value->exp));
    }
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejBoolean type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - boolean value.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackBool(const char* propertyName, bool value, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->append(value ? "true" : "false");
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejPropertyAnnotation type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
int callbackAnnotation(const char* propertyName, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    params->output->append("\"");
    params->output->append(propertyName);
    *params->isPrevAnnotated = true;
    return 0;
}

/**
 * @brief Callback for stackEmpty.
 *
 * @param[in] dataPtr - pointer to a valid std::vector<BejStackProperty>
 * @return true if the stack is empty.
 */
bool stackEmpty(void* dataPtr)
{
    std::vector<BejStackProperty>* stack =
        reinterpret_cast<std::vector<BejStackProperty>*>(dataPtr);
    return stack->empty();
}

/**
 * @brief Callback for stackPeek.
 *
 * @param[in] dataPtr - pointer to a valid std::vector<BejStackProperty>
 * @return a const reference to the stack top.
 */
const struct BejStackProperty* stackPeek(void* dataPtr)
{
    std::vector<BejStackProperty>* stack =
        reinterpret_cast<std::vector<BejStackProperty>*>(dataPtr);
    if (stack->empty())
    {
        return NULL;
    }
    return &(stack->back());
}

/**
 * @brief Callback for stackPop. Remove the top element from the stack.
 *
 * @param[in] dataPtr - pointer to a valid std::vector<BejStackProperty>
 */
void stackPop(void* dataPtr)
{
    std::vector<BejStackProperty>* stack =
        reinterpret_cast<std::vector<BejStackProperty>*>(dataPtr);
    if (stack->empty())
    {
        return;
    }
    stack->pop_back();
}

/**
 * @brief Callback for stackPush. Push a new element to the top of the stack.
 *
 * @param[in] property - property to push.
 * @param[in] dataPtr - pointer to a valid std::vector<BejStackProperty>
 * @return 0 if successful.
 */
int stackPush(const struct BejStackProperty* const property, void* dataPtr)
{
    std::vector<BejStackProperty>* stack =
        reinterpret_cast<std::vector<BejStackProperty>*>(dataPtr);
    stack->push_back(*property);
    return 0;
}

int BejDecoderJson::decode(const BejDictionaries& dictionaries,
                           const std::span<const uint8_t> encodedPldmBlock)
{
    // Clear the previous output if any.
    output.clear();

    // The dictionaries have to be traversed in a depth first manner. I am using
    // a stack to implement it non-recursively. Going into a set or an array or
    // a property annotation section means that we have to jump to the child
    // dictionary offset start point but needs to retrieve the parent dictionary
    // offset start once all the children are processed. This stack will hold
    // the parent dictionary offsets and endings for each section.
    stack.clear();

    struct BejStackCallback stackCallback = {
        .stackEmpty = stackEmpty,
        .stackPeek = stackPeek,
        .stackPop = stackPop,
        .stackPush = stackPush,
    };

    struct BejDecodedCallback decodedCallback = {
        .callbackSetStart = callbackSetStart,
        .callbackSetEnd = callbackSetEnd,
        .callbackArrayStart = callbackArrayStart,
        .callbackArrayEnd = callbackArrayEnd,
        .callbackPropertyEnd = callbackPropertyEnd,
        .callbackNull = callbackNull,
        .callbackInteger = callbackInteger,
        .callbackEnum = callbackEnum,
        .callbackString = callbackString,
        .callbackReal = callbackReal,
        .callbackBool = callbackBool,
        .callbackAnnotation = callbackAnnotation,
        .callbackReadonlyProperty = nullptr,
    };

    isPrevAnnotated = false;
    struct BejJsonParam callbackData = {
        .isPrevAnnotated = &isPrevAnnotated,
        .output = &output,
    };

    return bejDecodePldmBlock(&dictionaries, encodedPldmBlock.data(),
                              encodedPldmBlock.size_bytes(), &stackCallback,
                              &decodedCallback, (void*)(&callbackData),
                              (void*)(&stack));
}

std::string BejDecoderJson::getOutput()
{
    return output;
}

} // namespace libbej