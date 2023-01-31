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
