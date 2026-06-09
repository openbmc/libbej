#include "bej_encoder_json.hpp"

#include "bej_deferred_binding_format.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

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

// Records a leaf whose value was temporarily rewritten to a deferred binding
// placeholder, so encode() can restore the caller's tree after encoding.
struct DeferredBindingRestore
{
    struct RedfishPropertyLeafString* leaf;
    const char* originalValue;
    bool originalDeferredBinding;
};

// Transparent hash so a reverse-map lookup keyed by a C string/string_view does
// not allocate a temporary std::string per leaf.
struct DeferredBindingValueHash
{
    using is_transparent = void;
    size_t operator()(std::string_view value) const noexcept
    {
        return std::hash<std::string_view>{}(value);
    }
};

// Maps a property value to the deferred binding placeholder it should be
// encoded as. Built once per encode so each leaf is an O(1) lookup instead of a
// scan over the whole binding map.
using ReverseDeferredBindingMap =
    std::unordered_map<std::string, std::string, DeferredBindingValueHash,
                       std::equal_to<>>;

static ReverseDeferredBindingMap buildReverseDeferredBindingMap(
    const BejDeferredBindingMap& bindings)
{
    ReverseDeferredBindingMap reverse;
    for (const auto& [token, value] : bindings)
    {
        // First insertion wins when distinct placeholders share a value; map
        // iteration order is unspecified, so callers should keep values unique.
        // Empty values never produce a placeholder.
        if (!value.empty())
        {
            reverse.emplace(
                value, std::format("{}{}", bejDeferredBindingMarker, token));
        }
    }
    return reverse;
}

static void updateTreeForDeferredBindings(
    struct RedfishPropertyNode* node, const ReverseDeferredBindingMap& reverse,
    std::deque<std::string>& storage,
    std::vector<DeferredBindingRestore>& restores)
{
    if (!node || reverse.empty())
    {
        return;
    }

    if (bejTreeIsParentType(node))
    {
        struct RedfishPropertyParent* parent =
            reinterpret_cast<struct RedfishPropertyParent*>(node);
        struct RedfishPropertyNode* child =
            reinterpret_cast<struct RedfishPropertyNode*>(parent->firstChild);
        while (child != nullptr)
        {
            updateTreeForDeferredBindings(child, reverse, storage, restores);
            child = reinterpret_cast<struct RedfishPropertyNode*>(
                bejParentGoToNextChild(parent, child));
        }
        return;
    }

    // Only bejString leaves carry substitutable values on the encode side; the
    // encoder core has no bejResourceLink leaf type to emit.
    if (node->format.principalDataType != bejString)
    {
        return;
    }

    struct RedfishPropertyLeafString* leafStr =
        reinterpret_cast<struct RedfishPropertyLeafString*>(node);
    if (!leafStr->value)
    {
        return;
    }

    auto match = reverse.find(std::string_view{leafStr->value});
    if (match == reverse.end())
    {
        return;
    }

    // Own the placeholder for the encode lifetime via `storage`; a deque keeps
    // element addresses stable, so `value` may point at its c_str(). Record the
    // original pointer and flag so encode() can restore the caller's tree.
    restores.push_back(
        {leafStr, leafStr->value, node->format.deferredBinding != 0});
    storage.push_back(match->second);
    leafStr->value = storage.back().c_str();

    bejTreeUpdateNodeFlags(node, /*deferredBinding=*/true,
                           node->format.readOnlyPropertyAndTopLevelAnnotation,
                           node->format.nullableProperty);
}

int BejEncoderJson::encode(const struct BejDictionaries* dictionaries,
                           enum BejSchemaClass schemaClass,
                           struct RedfishPropertyParent* root,
                           const BejDeferredBindingMap& bindings)
{
    // Temporarily rewrite eligible string values to deferred binding
    // placeholders. `storage` owns the placeholder strings for the encode
    // duration; `restores` lets us put the caller's tree back afterwards so
    // encode() leaves no side effect on the input.
    ReverseDeferredBindingMap reverseBindings =
        buildReverseDeferredBindingMap(bindings);
    std::deque<std::string> deferredBindingStorage;
    std::vector<DeferredBindingRestore> deferredBindingRestores;
    updateTreeForDeferredBindings(
        reinterpret_cast<struct RedfishPropertyNode*>(root), reverseBindings,
        deferredBindingStorage, deferredBindingRestores);

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

    int rc = bejEncode(dictionaries, BEJ_DICTIONARY_START_AT_HEAD, schemaClass,
                       root, &output, &stackCallbacks);

    // Restore the caller's tree (original value pointers and deferred binding
    // flags); placeholders in `deferredBindingStorage` are unreferenced once
    // this returns.
    for (const DeferredBindingRestore& entry : deferredBindingRestores)
    {
        struct RedfishPropertyNode* node =
            reinterpret_cast<struct RedfishPropertyNode*>(entry.leaf);
        bejTreeUpdateNodeFlags(
            node, entry.originalDeferredBinding,
            node->format.readOnlyPropertyAndTopLevelAnnotation,
            node->format.nullableProperty);
        entry.leaf->value = entry.originalValue;
    }

    return rc;
}

} // namespace libbej
