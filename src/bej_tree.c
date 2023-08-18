#include "bej_tree.h"

static void bejTreeInitParent(struct RedfishPropertyParent* node,
                              const char* name, enum BejPrincipalDataType type)
{
    node->nodeAttr.name = name;
    node->nodeAttr.format.principalDataType = type;
    node->nodeAttr.format.deferredBinding = 0;
    node->nodeAttr.format.readOnlyProperty = 0;
    node->nodeAttr.format.nullableProperty = 0;
    node->nodeAttr.sibling = NULL;
    node->nChildren = 0;
    node->firstChild = NULL;
    node->lastChild = NULL;
}

void bejTreeInitSet(struct RedfishPropertyParent* node, const char* name)
{
    bejTreeInitParent(node, name, bejSet);
}

void bejTreeInitArray(struct RedfishPropertyParent* node, const char* name)
{
    bejTreeInitParent(node, name, bejArray);
}

void bejTreeInitPropertyAnnotated(struct RedfishPropertyParent* node,
                                  const char* name)
{
    bejTreeInitParent(node, name, bejPropertyAnnotation);
}

bool bejTreeIsParentType(struct RedfishPropertyNode* node)
{
    return node->format.principalDataType == bejSet ||
           node->format.principalDataType == bejArray ||
           node->format.principalDataType == bejPropertyAnnotation;
}

static void bejTreeInitChildNode(struct RedfishPropertyLeaf* node,
                                 const char* name,
                                 enum BejPrincipalDataType type)
{
    node->nodeAttr.name = name;
    node->nodeAttr.format.principalDataType = type;
    node->nodeAttr.format.deferredBinding = 0;
    node->nodeAttr.format.readOnlyProperty = 0;
    node->nodeAttr.format.nullableProperty = 0;
    node->nodeAttr.sibling = NULL;
}

void bejTreeAddNull(struct RedfishPropertyParent* parent,
                    struct RedfishPropertyLeafNull* child, const char* name)
{
    bejTreeInitChildNode((struct RedfishPropertyLeaf*)child, name, bejNull);
    bejTreeLinkChildToParent(parent, child);
}

void bejTreeAddInteger(struct RedfishPropertyParent* parent,
                       struct RedfishPropertyLeafInt* child, const char* name,
                       int64_t value)
{
    bejTreeInitChildNode((struct RedfishPropertyLeaf*)child, name, bejInteger);
    child->value = value;
    bejTreeLinkChildToParent(parent, child);
}

void bejTreeSetInteger(struct RedfishPropertyLeafInt* node, int64_t newValue)
{
    node->value = newValue;
}

void bejTreeAddEnum(struct RedfishPropertyParent* parent,
                    struct RedfishPropertyLeafEnum* child, const char* name,
                    const char* value)
{
    bejTreeInitChildNode((struct RedfishPropertyLeaf*)child, name, bejEnum);
    child->value = value;
    bejTreeLinkChildToParent(parent, child);
}

void bejTreeAddString(struct RedfishPropertyParent* parent,
                      struct RedfishPropertyLeafString* child, const char* name,
                      const char* value)
{
    bejTreeInitChildNode((struct RedfishPropertyLeaf*)child, name, bejString);
    child->value = value;
    bejTreeLinkChildToParent(parent, child);
}

void bejTreeAddReal(struct RedfishPropertyParent* parent,
                    struct RedfishPropertyLeafReal* child, const char* name,
                    double value)
{
    bejTreeInitChildNode((struct RedfishPropertyLeaf*)child, name, bejReal);
    child->value = value;
    bejTreeLinkChildToParent(parent, child);
}

void bejTreeSetReal(struct RedfishPropertyLeafReal* node, double newValue)
{
    node->value = newValue;
}

void bejTreeAddBool(struct RedfishPropertyParent* parent,
                    struct RedfishPropertyLeafBool* child, const char* name,
                    bool value)
{
    bejTreeInitChildNode((struct RedfishPropertyLeaf*)child, name, bejBoolean);
    child->value = value;
    bejTreeLinkChildToParent(parent, child);
}

void bejTreeLinkChildToParent(struct RedfishPropertyParent* parent, void* child)
{
    // A new node is added at the end of the list.
    if (parent->firstChild == NULL)
    {
        parent->firstChild = child;
    }
    else
    {
        struct RedfishPropertyNode* lastChild = parent->lastChild;
        lastChild->sibling = child;
    }
    parent->lastChild = child;
    parent->nChildren += 1;
}

void bejTreeUpdateNodeFlags(struct RedfishPropertyNode* node,
                            bool deferredBinding, bool readOnlyProperty,
                            bool nullableProperty)
{
    node->format.deferredBinding = deferredBinding;
    node->format.readOnlyProperty = readOnlyProperty;
    node->format.nullableProperty = nullableProperty;
}

void* bejParentGoToNextChild(struct RedfishPropertyParent* parent,
                             struct RedfishPropertyNode* currentChild)
{
    if (parent == NULL || currentChild == NULL)
    {
        return NULL;
    }

    parent->metaData.nextChildIndex += 1;
    parent->metaData.nextChild = currentChild->sibling;
    return currentChild->sibling;
}

int bejAddLinkToJson(struct RedfishPropertyParent* parent,
                     struct RedfishLinkJson* linkJson, const char* linkSetName,
                     const char* value)
{
    // Nothing to be added.
    if (value == NULL)
    {
        return 0;
    }

    NULL_CHECK(parent, "Parent can't be NULL");
    NULL_CHECK(linkJson, "Link JSON storage can't be NULL");

    bejTreeInitSet(&linkJson->set, linkSetName);
    bejTreeAddString(&linkJson->set, &linkJson->odataId, "@odata.id", value);
    bejTreeLinkChildToParent(parent, &linkJson->set);
    return 0;
}

int bejAddAnnotatedCountToJson(struct RedfishPropertyParent* parent,
                               struct RedfishPropertyParent* annotatedProperty,
                               const char* annotatedPropertyName,
                               struct RedfishPropertyLeafInt* countProperty,
                               int64_t value)
{
    NULL_CHECK(parent, "Parent can't be NULL");
    NULL_CHECK(annotatedProperty,
               "Storage for property being annotated can't be NULL");
    NULL_CHECK(countProperty, "Storage for count property can't be NULL");

    bejTreeInitPropertyAnnotated(annotatedProperty, annotatedPropertyName);
    bejTreeAddInteger(annotatedProperty, countProperty, "@odata.count", value);
    bejTreeLinkChildToParent(parent, annotatedProperty);
    return 0;
}

int bejCreateArrayOfLinksJson(struct RedfishPropertyParent* parent,
                              const char* arrayName, uint16_t linkCount,
                              const char* const links[],
                              struct RedfishArrayOfLinksJson* linksInfo,
                              struct RedfishLinkJson* linkJsonArray)
{
    NULL_CHECK(parent, "Invalid parent pointer for creating an array of links");
    NULL_CHECK(linksInfo, "NULL pointer for links array metadata JSON storage");

    // Link the array to the parent node.
    bejTreeInitArray(&linksInfo->array, arrayName);
    bejTreeLinkChildToParent(parent, &linksInfo->array);

    RETURN_IF_IERROR(
        bejAddAnnotatedCountToJson(parent, &linksInfo->annotatedProperty,
                                   arrayName, &linksInfo->count, linkCount));

    // No links to be addded.
    if (linkCount == 0)
    {
        return 0;
    }

    NULL_CHECK(links, "Links array of strings can't be NULL");
    NULL_CHECK(linkJsonArray, "NULL pointer for links JSON storage");

    for (uint16_t i = 0; i < linkCount; ++i)
    {
        RETURN_IF_IERROR(bejAddLinkToJson(&linksInfo->array, &linkJsonArray[i],
                                          NULL, links[i]));
    }
    return 0;
}
