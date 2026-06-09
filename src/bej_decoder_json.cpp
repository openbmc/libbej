#include "bej_decoder_json.hpp"

#include "bej_common.h"
#include "bej_decoder_core.h"

#include "bej_deferred_binding_format.hpp"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <string>

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
    const libbej::BejDeferredBindingMap* bindings;
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
 * @brief Parse a base-10 integer at str[start] without throwing.
 *
 * @param[in] str - the string to parse from (start must be <= str.length()).
 * @param[in] start - index to begin parsing.
 * @param[out] out - parsed value, valid only on success.
 * @param[out] end - index just past the last parsed digit.
 * @return true if at least one digit was parsed and the value fit in T.
 */
template <typename T>
static bool parseDecimalAt(const std::string& str, size_t start, T& out,
                           size_t& end)
{
    auto result =
        std::from_chars(str.data() + start, str.data() + str.length(), out);
    end = static_cast<size_t>(result.ptr - str.data());
    return result.ec == std::errc{};
}

/**
 * @brief Resolve a resource id to its URI substitution. Shared by the %L
 * placeholder path and the numeric bejResourceLink path.
 *
 * @param[in] bindings - resource id to substitution parameters.
 * @param[in] resourceId - resource id to look up.
 * @return the URI, or nullopt if the resource is absent or its URI is empty.
 */
static std::optional<std::string> resolveResourceUri(
    const BejDeferredBindingMap& bindings, uint32_t resourceId)
{
    auto it = bindings.find(resourceId);
    if (it == bindings.end() || it->second.uri.empty())
    {
        return std::nullopt;
    }
    return it->second.uri;
}

/**
 * @brief Resolve one deferred binding placeholder to its substitution value.
 *
 * @param[in] bindings - resource id to substitution parameters.
 * @param[in] resourceId - parsed resource id from the placeholder.
 * @param[in] subType - substitution character following the '%'.
 * @param[in] str - the string being substituted.
 * @param[in,out] idEnd - index just past the resource id; advanced past the
 * parsed action id on a %T<id>.<action> match.
 * @return the replacement, or nullopt to leave the placeholder untouched. A
 * missing resource, missing action or empty mapping is never substituted.
 */
static std::optional<std::string> resolveDeferredBinding(
    const BejDeferredBindingMap& bindings, uint32_t resourceId, char subType,
    const std::string& str, size_t& idEnd)
{
    if (subType == bejDeferredBindingResourceLink)
    {
        return resolveResourceUri(bindings, resourceId);
    }

    auto it = bindings.find(resourceId);
    if (it == bindings.end())
    {
        return std::nullopt;
    }
    const BejDeferredBindingResource& resource = it->second;

    if (subType == bejDeferredBindingInstanceId)
    {
        if (resource.instanceId.empty())
        {
            return std::nullopt;
        }
        return resource.instanceId;
    }
    if (subType == bejDeferredBindingActionUri)
    {
        // Action URI expects %T<id>.<action>.
        if (idEnd >= str.length() ||
            str[idEnd] != bejDeferredBindingActionSeparator)
        {
            return std::nullopt;
        }
        uint8_t actionId = 0;
        size_t actEnd = 0;
        if (!parseDecimalAt(str, idEnd + 1, actionId, actEnd))
        {
            return std::nullopt;
        }
        auto actIt = resource.actionUris.find(actionId);
        if (actIt == resource.actionUris.end() || actIt->second.empty())
        {
            return std::nullopt;
        }
        // Advance past the parsed action id so the whole placeholder is
        // replaced.
        idEnd = actEnd;
        return actIt->second;
    }
    // Custom/future DSP0218 substitutions.
    auto otherIt = resource.otherParameters.find(subType);
    if (otherIt == resource.otherParameters.end() || otherIt->second.empty())
    {
        return std::nullopt;
    }
    return otherIt->second;
}

/**
 * @brief Substitute deferred binding placeholders in a string and append the
 * result to the output.
 */
static void addDeferredBindingString(struct BejJsonParam* params,
                                     const char* value, size_t length)
{
    if (!params->bindings || params->bindings->empty())
    {
        // No bindings provided, just output the raw string.
        if (length > 0)
        {
            params->output->append(value, length - 1);
        }
        return;
    }

    std::string str(value, length > 0 ? length - 1 : 0);
    size_t pos = 0;
    while ((pos = str.find(bejDeferredBindingMarker, pos)) != std::string::npos)
    {
        if (pos + 1 < str.length() && str[pos + 1] == bejDeferredBindingMarker)
        {
            // Escaped marker. Replace "%%" with "%".
            str.replace(pos, 2, 1, bejDeferredBindingMarker);
            pos++;
            continue;
        }

        if (pos + 1 >= str.length())
        {
            pos++;
            continue;
        }

        char subType = str[pos + 1];

        // Parse the resource id without throwing (from_chars reports overflow
        // and "no digits" via its return, unlike std::stoul whose exception
        // would unwind across the C decoder frames). idEnd marks the first
        // non-digit, where an optional ".<action>" suffix may begin.
        uint32_t resourceId = 0;
        size_t idEnd = 0;
        if (parseDecimalAt(str, pos + 2, resourceId, idEnd))
        {
            if (auto replacement = resolveDeferredBinding(
                    *params->bindings, resourceId, subType, str, idEnd))
            {
                str.replace(pos, idEnd - pos, *replacement);
                pos += replacement->length();
                continue;
            }
        }
        pos += 2; // skip % and subtype
    }
    params->output->append(str);
}

/**
 * @brief Callback for bejString type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] value - a NULL terminated string.
 * @param[in] length - length of the string.
 * @param[in] deferredBinding - indicates if string contains deferred binding
 * placeholders.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackString(const char* propertyName, const char* value,
                          size_t length, bool deferredBinding, void* dataPtr)
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
    if (deferredBinding)
    {
        addDeferredBindingString(params, value, length);
    }
    else if (length > 0)
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
 * @brief Callback for bejResourceLink type.
 *
 * @param[in] propertyName - a NULL terminated string.
 * @param[in] linkId - Resource Link ID.
 * @param[in] dataPtr - pointing to a valid BejJsonParam struct.
 * @return 0 if successful.
 */
static int callbackResourceLink(const char* propertyName, uint64_t linkId,
                                void* dataPtr)
{
    struct BejJsonParam* params =
        reinterpret_cast<struct BejJsonParam*>(dataPtr);
    addPropertyNameToOutput(params, propertyName);

    // bejResourceLink carries a numeric resource id, not a placeholder string,
    // so it is resolved here rather than inside addDeferredBindingString. The
    // map is keyed by uint32_t; a link id beyond that range cannot match and
    // must fall through to the raw placeholder rather than alias a 32-bit id.
    if (params->bindings && linkId <= std::numeric_limits<uint32_t>::max())
    {
        if (auto uri = resolveResourceUri(*params->bindings,
                                          static_cast<uint32_t>(linkId)))
        {
            params->output->append(std::format("\"{}\"", *uri));
            *params->isPrevAnnotated = false;
            return 0;
        }
    }

    // Fallback: no binding found, emit the raw %L<id> placeholder.
    params->output->append(std::format(
        "\"{}\"",
        bejFormatDeferredBinding(bejDeferredBindingResourceLink, linkId)));
    *params->isPrevAnnotated = false;
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
                           const std::span<const uint8_t> encodedPldmBlock,
                           const BejDeferredBindingMap& bindings)
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
        .callbackResourceLink = callbackResourceLink,
        .callbackReadonlyProperty = nullptr,
    };

    isPrevAnnotated = false;
    struct BejJsonParam callbackData = {
        .isPrevAnnotated = &isPrevAnnotated,
        .output = &output,
        .bindings = &bindings,
    };

    return bejDecodePldmBlockWithPolicy(
        &dictionaries, encodedPldmBlock.data(), encodedPldmBlock.size_bytes(),
        &stackCallback, &decodedCallback, (void*)(&callbackData),
        (void*)(&stack), trailingPolicy);
}

std::string BejDecoderJson::getOutput()
{
    return output;
}

} // namespace libbej
