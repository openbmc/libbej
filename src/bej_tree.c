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
