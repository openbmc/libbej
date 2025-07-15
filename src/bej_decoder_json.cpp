#include "bej_decoder_json.hpp"

#include <string.h>

#define MAX_BEJ_STRING_LEN 65536

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
        params->output->push_back('\"');
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
    params->output->push_back('{');
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
    params->output->push_back('}');
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
    params->output->push_back('[');
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
    params->output->push_back(']');
    return 0;
}

/**
 * @brief Callback when an end of a property is detected.
 *
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackPropertyEnd(void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    // Not a section ending. So add a comma.
    params->output->push_back(',');
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
static int callbackInteger(const char* propertyName, int64_t value,
                           void* dataPtr)
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
static int callbackEnum(const char* propertyName, const char* value,
                        void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->push_back('\"');
    params->output->append(value);
    params->output->push_back('\"');
    *params->isPrevAnnotated = false;
    return 0;
}

/**
 * @brief Callback for bejString type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - a NULL terminated string.
 * @param[in] length - length of the string.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackString(const char* propertyName, const char* value,
                          size_t length, void* dataPtr)
{
    if ((length > MAX_BEJ_STRING_LEN) ||
        (strnlen(value, length) != (length - 1)))
    {
        fprintf(stderr,
                "Incorrect BEJ string length %zu or it exceeds maximum %u.\n",
                (length - 1), MAX_BEJ_STRING_LEN);
        return bejErrorInvalidSize;
    }
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);
    params->output->push_back('\"');
    if (length > 0)
    {
        params->output->append(value, length - 1);
    }
    params->output->push_back('\"');
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
static int callbackReal(const char* propertyName, const struct BejReal* value,
                        void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);

    // Sanity check for zeroCount
    if (value->zeroCount > 100)
    {
        return bejErrorInvalidSize;
    }

    addPropertyNameToOutput(params, propertyName);
    params->output->append(std::to_string(value->whole));
    params->output->push_back('.');
    params->output->insert(params->output->cend(), value->zeroCount, '0');
    params->output->append(std::to_string(value->fract));
    if (value->expLen != 0)
    {
        params->output->push_back('e');
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
static int callbackBool(const char* propertyName, bool value, void* dataPtr)
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
static int callbackAnnotation(const char* propertyName, void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    params->output->push_back('\"');
    params->output->append(propertyName);

    // bejPropertyAnnotation type has the form "Status@Message.ExtendedInfo".
    // First the decoder will see "Status" part of the annotated property. This
    // will be in its own SFLV tuple. The remainder of the property name,
    // @Message.ExtendedInfo will be contained in the next bej SFLV tuple.
    // Therefore to add the inverted commas to the complete property name,
    // Status@Message.ExtendedInfo, we need to know that the previous property
    // we processed is a start to an annotation property. We can use
    // isPrevAnnotated to pass this information.
    // Here we are adding: "propertyName
    // If isPrevAnnotated is true, next property should add: propertyNameNext"
    *params->isPrevAnnotated = true;
    return 0;
}

/**
 * @brief Callback for stackEmpty.
 *
 * @param[in] dataPtr - pointer to a valid std::vector<BejStackProperty>
 * @return true if the stack is empty.
 */
static bool stackEmpty(void* dataPtr)
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
static const struct BejStackProperty* stackPeek(void* dataPtr)
{
    std::vector<BejStackProperty>* stack =
        reinterpret_cast<std::vector<BejStackProperty>*>(dataPtr);
    if (stack->empty())
    {
        return nullptr;
    }
    return &(stack->back());
}

/**
 * @brief Callback for stackPop. Remove the top element from the stack.
 *
 * @param[in] dataPtr - pointer to a valid std::vector<BejStackProperty>
 */
static void stackPop(void* dataPtr)
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
static int stackPush(const struct BejStackProperty* const property,
                     void* dataPtr)
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

    // The dictionaries have to be traversed in a depth first manner. This is
    // using a stack to implement it non-recursively. Going into a set or an
    // array or a property annotation section means that we have to jump to the
    // child dictionary offset start point but needs to retrieve the parent
    // dictionary offset start once all the children are processed. This stack
    // will hold the parent dictionary offsets and endings for each section.
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

    return bejDecodePldmBlock(
        &dictionaries, encodedPldmBlock.data(), encodedPldmBlock.size_bytes(),
        &stackCallback, &decodedCallback, (void*)(&callbackData),
        (void*)(&stack));
}

std::string BejDecoderJson::getOutput()
{
    return output;
}

} // namespace libbej
