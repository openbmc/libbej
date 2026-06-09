#include "bej_decoder_json.hpp"

#include "bej_common.h"
#include "bej_decoder_core.h"

#include "bej_deferred_binding_format.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

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
    const BejDeferredBindingMap* bindings;
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

// DSP0218 Table 42 markers supported by this decoder, modelled as data so
// adding a marker is a new table row rather than a new code branch.
namespace
{

// How a marker's trailing parameters are parsed, which also fixes the token
// boundary.
enum class BejMacroArity : uint8_t
{
    none,                // %C, %S: the token is just the marker character
    resourceId,          // %L<id>, %I<id>: marker + decimal id
    resourceIdDotAction, // %T<id>.<action>: marker + decimal id + '.' + action
};

// Shape of the DSP0218 Table 42 "invalid" form, emitted when resolution is
// enabled (a non-empty map) but the placeholder cannot be resolved.
enum class BejInvalidKind : uint8_t
{
    none,        // %C, %S: spec defines no invalid form; leave the token raw
    resourcePdr, // %L<id>     -> "/invalid.PDR<id>"
    literal,     // %I<id>     -> "invalid"
    action,      // %T<id>.<n> -> "/invalid.<id>.<n>"
};

struct BejMacroSpec
{
    char marker;
    BejMacroArity arity;
    BejInvalidKind invalid;
};

constexpr auto bejMacroSpecs = std::to_array<BejMacroSpec>({
    {bejDeferredBindingResourceLink, BejMacroArity::resourceId,
     BejInvalidKind::resourcePdr},
    {bejDeferredBindingInstanceId, BejMacroArity::resourceId,
     BejInvalidKind::literal},
    {bejDeferredBindingActionUri, BejMacroArity::resourceIdDotAction,
     BejInvalidKind::action},
    {bejDeferredBindingChassis, BejMacroArity::none, BejInvalidKind::none},
    {bejDeferredBindingSystem, BejMacroArity::none, BejInvalidKind::none},
});

constexpr std::string_view bejInvalidPdrPrefix = "/invalid.PDR";
constexpr std::string_view bejInvalidLiteral = "invalid";
constexpr std::string_view bejInvalidActionPrefix = "/invalid.";

const BejMacroSpec* findMacroSpec(char marker)
{
    for (const BejMacroSpec& spec : bejMacroSpecs)
    {
        if (spec.marker == marker)
        {
            return &spec;
        }
    }
    return nullptr;
}

// Parse the placeholder body that begins at the marker character (one past the
// '%'). On success, bodyEnd is the index just past the token, and resourceId /
// actionId hold the parsed numbers per the marker's arity. Returns false for a
// malformed body, which the caller leaves raw.
bool parseMacroBody(const std::string& str, size_t markerPos,
                    const BejMacroSpec& spec, size_t& bodyEnd,
                    uint32_t& resourceId, uint32_t& actionId)
{
    if (spec.arity == BejMacroArity::none)
    {
        bodyEnd = markerPos + 1;
        return true;
    }
    // from_chars reports overflow and "no digits" via its return, unlike
    // std::stoul whose exception would unwind across the C decoder frames.
    size_t idEnd = 0;
    if (!parseDecimalAt(str, markerPos + 1, resourceId, idEnd))
    {
        return false;
    }
    if (spec.arity == BejMacroArity::resourceId)
    {
        bodyEnd = idEnd;
        return true;
    }
    // resourceIdDotAction requires a ".<action>" suffix.
    if (idEnd >= str.length() ||
        str[idEnd] != bejDeferredBindingActionSeparator)
    {
        return false;
    }
    size_t actEnd = 0;
    if (!parseDecimalAt(str, idEnd + 1, actionId, actEnd))
    {
        return false;
    }
    bodyEnd = actEnd;
    return true;
}

// Build the DSP0218 Table 42 invalid form for an unresolved placeholder.
// Returns false when the marker has no invalid form (%C, %S), leaving it raw.
bool buildInvalidForm(const BejMacroSpec& spec, uint32_t resourceId,
                      uint32_t actionId, std::string& out)
{
    switch (spec.invalid)
    {
        case BejInvalidKind::resourcePdr:
            out = std::format("{}{}", bejInvalidPdrPrefix, resourceId);
            return true;
        case BejInvalidKind::literal:
            out = std::string(bejInvalidLiteral);
            return true;
        case BejInvalidKind::action:
            out = std::format("{}{}{}{}", bejInvalidActionPrefix, resourceId,
                              bejDeferredBindingActionSeparator, actionId);
            return true;
        case BejInvalidKind::none:
            return false;
    }
    return false;
}

} // namespace

/**
 * @brief Substitute deferred binding placeholders in a string and append the
 * result to the output.
 */
static void addDeferredBindingString(struct BejJsonParam* params,
                                     const char* value, size_t length)
{
    if (length == 0)
    {
        return;
    }
    if (!params->bindings || params->bindings->empty())
    {
        // Resolution disabled: emit the raw string unchanged.
        params->output->append(value, length - 1);
        return;
    }

    std::string str(value, length - 1);
    size_t pos = 0;
    while ((pos = str.find(bejDeferredBindingMarker, pos)) != std::string::npos)
    {
        if (pos + 1 >= str.length())
        {
            // A lone '%' at the end of the string: no marker can follow.
            break;
        }
        if (str[pos + 1] == bejDeferredBindingMarker)
        {
            // Escaped marker. Replace "%%" with "%".
            str.replace(pos, 2, 1, bejDeferredBindingMarker);
            pos++;
            continue;
        }

        const BejMacroSpec* spec = findMacroSpec(str[pos + 1]);
        if (spec == nullptr)
        {
            // Unsupported marker: DSP0218 Table 42 passes it through unchanged.
            pos++;
            continue;
        }

        size_t bodyEnd = 0;
        uint32_t resourceId = 0;
        uint32_t actionId = 0;
        if (!parseMacroBody(str, pos + 1, *spec, bodyEnd, resourceId, actionId))
        {
            // Malformed body: leave the placeholder raw.
            pos++;
            continue;
        }

        // The lookup token is the placeholder body (e.g. "L30", "T30.1", "C").
        std::string token = str.substr(pos + 1, bodyEnd - (pos + 1));
        std::string replacement;
        auto it = params->bindings->find(token);
        if (it != params->bindings->end() && !it->second.empty())
        {
            replacement = it->second;
        }
        else if (!buildInvalidForm(*spec, resourceId, actionId, replacement))
        {
            // No binding and no invalid form (%C, %S): leave the token raw.
            pos = bodyEnd;
            continue;
        }
        str.replace(pos, bodyEnd - pos, replacement);
        pos += replacement.length();
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
    if (length > 0)
    {
        if (deferredBinding)
        {
            addDeferredBindingString(params, value, length);
        }
        else
        {
            params->output->append(value, length - 1);
        }
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
    *params->isPrevAnnotated = false;

    // bejResourceLink carries a numeric resource id, not a placeholder string,
    // so it resolves here rather than inside addDeferredBindingString.
    const bool resolutionEnabled =
        params->bindings && !params->bindings->empty();
    if (!resolutionEnabled)
    {
        // Resolution disabled (no bindings supplied): preserve the raw %L<id>
        // placeholder, matching decode() called without a map.
        params->output->append(std::format("\"{}{}{}\"",
                                            bejDeferredBindingMarker,
                                            bejDeferredBindingResourceLink,
                                            linkId));
        return 0;
    }

    // Resource ids are 32-bit; a link id beyond that range cannot name a
    // resource, so it never matches and falls through to the invalid form
    // rather than being truncated into a 32-bit token.
    if (linkId <= std::numeric_limits<uint32_t>::max())
    {
        auto it = params->bindings->find(
            bejBinding::resourceLink(static_cast<uint32_t>(linkId)));
        if (it != params->bindings->end() && !it->second.empty())
        {
            params->output->append(std::format("\"{}\"", it->second));
            return 0;
        }
    }

    // Enabled but unrecognized (or out of 32-bit range): %L invalid form.
    params->output->append(std::format("\"{}{}\"", bejInvalidPdrPrefix, linkId));
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
