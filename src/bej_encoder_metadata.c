#include "bej_encoder_metadata.h"

#include "bej_common.h"
#include "bej_dictionary.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Update metadata of leaf nodes.
 *
 * @param dictionaries - dictionaries needed for encoding.
 * @param parentDictionary - dictionary used by this node's parent.
 * @param childPtr - a pointer to the leaf node.
 * @param childIndex - if this node is an array element, this is the array
 * index.
 * @param dictStartingOffset - starting dictionary child offset value of this
 * node's parent.
 * @return 0 if successful.
 */
static int bejUpdateLeafNodeMetaData(const struct BejDictionaries* dictionaries,
                                     const uint8_t* parentDictionary,
                                     void* childPtr, uint16_t childIndex,
                                     uint16_t dictStartingOffset)
{
    // TODO: Implement this
    (void)dictionaries;
    (void)parentDictionary;
    (void)childIndex;
    (void)dictStartingOffset;

    struct RedfishPropertyLeaf* chNode = childPtr;
    switch (chNode->nodeAttr.format.principalDataType)
    {
        default:
            fprintf(stderr, "Child type %u not supported\n",
                    chNode->nodeAttr.format.principalDataType);
            return -1;
    }
    return 0;
}

/**
 * @brief Update metadata of a parent node.
 *
 * @param dictionaries - dictionaries needed for encoding.
 * @param parentDictionary - dictionary used by this node's parent.
 * @param dictStartingOffset - starting dictionary child offset value of this
 * node's parent.
 * @param node - a pointer to the parent node.
 * @param nodeIndex - If this node is an array element, this is the array index.
 * @return 0 if successful.
 */
static int bejUpdateParentMetaData(const struct BejDictionaries* dictionaries,
                                   const uint8_t* parentDictionary,
                                   uint16_t dictStartingOffset,
                                   struct RedfishPropertyParent* node,
                                   uint16_t nodeIndex)
{
    // TODO: Implement this
    (void)dictionaries;
    (void)parentDictionary;
    (void)dictStartingOffset;
    (void)node;
    (void)nodeIndex;

    return -1;
}

/**
 * @brief Update metadata of child nodes.
 *
 * If a child node contains its own child nodes, it will be added to the stack
 * and function will return.
 *
 * @param dictionaries - dictionaries needed for encoding.
 * @param parent - parent node.
 * @param stack - stack holding parent nodes.
 * @return 0 if successful.
 */
static int bejProcessChildNodes(const struct BejDictionaries* dictionaries,
                                struct RedfishPropertyParent* parent,
                                struct BejPointerStackCallback* stack)
{
    // Get the next child of the parent.
    void* childPtr = parent->metaData.nextChild;

    // Process all the children belongs to the parent.
    while (childPtr != NULL)
    {
        // If we find a child with its own child nodes, add it to the stack and
        // return.
        if (bejTreeIsParentType(childPtr))
        {
            RETURN_IF_IERROR(bejUpdateParentMetaData(
                dictionaries, parent->metaData.dictionary,
                parent->metaData.childrenDictPropOffset, childPtr,
                parent->metaData.nextChildIndex));

            RETURN_IF_IERROR(stack->stackPush(childPtr, stack->stackContext));
            bejParentGoToNextChild(parent, childPtr);
            return 0;
        }

        RETURN_IF_IERROR(
            bejUpdateLeafNodeMetaData(dictionaries, parent->metaData.dictionary,
                                      childPtr, parent->metaData.nextChildIndex,
                                      parent->metaData.childrenDictPropOffset));
        // Use the child value size to update the parent value size.
        struct RedfishPropertyLeaf* leafChild = childPtr;
        // V: Include the child size in parent's value size.
        parent->metaData.vSize +=
            (leafChild->metaData.sflSize + leafChild->metaData.vSize);

        // Get the next child belongs to the parent.
        childPtr = bejParentGoToNextChild(parent, childPtr);
    }
    return 0;
}

int bejUpdateNodeMetadata(const struct BejDictionaries* dictionaries,
                          uint16_t majorSchemaStartingOffset,
                          struct RedfishPropertyParent* root,
                          struct BejPointerStackCallback* stack)
{
    // Decide the starting property offset of the dictionary.
    uint16_t dictOffset = bejDictGetPropertyHeadOffset();
    if (majorSchemaStartingOffset != BEJ_DICTIONARY_START_AT_HEAD)
    {
        dictOffset = majorSchemaStartingOffset;
    }

    // Initialize root node metadata.
    RETURN_IF_IERROR(
        bejUpdateParentMetaData(dictionaries, dictionaries->schemaDictionary,
                                dictOffset, root, /*childIndex=*/0));

    // Push the root to the stack. Because we are not done with the parent node
    // yet. Need to figure out all bytes need to encode children of this parent,
    // and save it in the parent metadata.
    RETURN_IF_IERROR(stack->stackPush(root, stack->stackContext));

    while (!stack->stackEmpty(stack->stackContext))
    {
        // Get the parent at the top of the stack. Stack is only popped if the
        // parent stack entry has no pending children; That is
        // parent->metaData.nextChild == NULL.
        struct RedfishPropertyParent* parent =
            stack->stackPeek(stack->stackContext);

        // Calculate metadata of all the child nodes of the current parent node.
        // If one of these child nodes has its own child nodes, that child node
        // will be added to the stack and this function will return.
        RETURN_IF_IERROR(bejProcessChildNodes(dictionaries, parent, stack));

        // If a new node hasn't been added to the stack, we know that this
        // parent's child nodes have been processed. If not, do not pop the
        // stack.
        if (parent != stack->stackPeek(stack->stackContext))
        {
            continue;
        }

        // If we are here;
        // Then "parent" is the top element of the stack.
        // All the children of "parent" has been processed.

        // Remove the "parent" from the stack.
        parent = stack->stackPop(stack->stackContext);
        // L: Add the length needed to store the number of bytes used for the
        // parent's value.
        parent->metaData.sflSize +=
            bejNnintEncodingSizeOfUInt(parent->metaData.vSize);

        // Since we now know the total size needs to encode the node pointed by
        // "parent" variable, we should add that to the value size of this
        // node's parent. Since we already popped this node from the stack, top
        // of the stack element is this nodes's parent. "parentsParent" can be
        // NULL if the node pointed by "parent" variable is the root.
        struct RedfishPropertyParent* parentsParent =
            stack->stackPeek(stack->stackContext);
        if (parentsParent != NULL)
        {
            // V: Include the total size to encode the current parent in its
            // parent's value size.
            parentsParent->metaData.vSize +=
                (parent->metaData.sflSize + parent->metaData.vSize);
        }
    }
    return 0;
}
