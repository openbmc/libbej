#include "bej_encoder_metadata.h"

#include "bej_common.h"
#include "bej_dictionary.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Check the name is an annotation type name.
 *
 * @param[in] name - property name.
 * @return true for annotation name, false otherwise.
 */
static bool bejIsAnnotation(const char* name)
{
    if (name == NULL)
    {
        return false;
    }
    return name[0] == '@';
}

/**
 * @brief Get the dictionary for the provided node.
 *
 * @param[in] dictionaries - available dictionaries for encoding.
 * @param[in] parentDictionary - dictionary used for the parent of this node.
 * @param[in] nodeName - name of the interested node. Can be NULL if the node
 * doesn't have a name.
 * @return a pointer to the dictionary to be used.
 */
static const uint8_t*
    bejGetRelatedDictionary(const struct BejDictionaries* dictionaries,
                            const uint8_t* parentDictionary,
                            const char* nodeName)
{
    // If the node name is NULL, we have to use parent dictionary.
    if (nodeName == NULL)
    {
        return parentDictionary;
    }

    // If the parent is using annotation dictionary, that means the parent is an
    // annotation. Therefore the child (this node) should be an annotation too
    // (Could this be false?). Therefore we should use the annotation dictionary
    // for this node as well.
    if (parentDictionary == dictionaries->annotationDictionary)
    {
        return dictionaries->annotationDictionary;
    }
    return bejIsAnnotation(nodeName) ? dictionaries->annotationDictionary
                                     : dictionaries->schemaDictionary;
}

/**
 * @brief Get dictionary data for the given node.
 *
 * @param[in] dictionaries - available dictionaries.
 * @param[in] parentDictionary - the dictionary used by the provided node's
 * parent.
 * @param[in] node - node that caller is interested in.
 * @param[in] nodeIndex - index of this node within its parent.
 * @param[in] dictStartingOffset - starting dictionary child offset value of
 * this node's parent.
 * @param[out] sequenceNumber - sequence number of the node. bit0 specifies the
 * dictionary schema type: [major|annotation].
 * @param[out] nodeDictionary - if not NULL, return a pointer to the dictionary
 * used for the node.
 * @param[out] childEntryOffset - if not NULL, return the dictionary starting
 * offset used for this nodes children. If this node is not supposed to have
 * children, caller should ignore this value.
 * @return 0 if successful.
 */
static int bejFindSeqNumAndChildDictOffset(
    const struct BejDictionaries* dictionaries, const uint8_t* parentDictionary,
    struct RedfishPropertyNode* node, uint16_t nodeIndex,
    uint16_t dictStartingOffset, uint32_t* sequenceNumber,
    const uint8_t** nodeDictionary, uint16_t* childEntryOffset)
{
    // If the node doesn't have a name, we can't use a dictionary. So we can use
    // its parent's info.
    if (node->name == NULL || node->name[0] == '\0')
    {
        if (nodeDictionary != NULL)
        {
            *nodeDictionary = parentDictionary;
        }

        if (childEntryOffset != NULL)
        {
            *childEntryOffset = dictStartingOffset;
        }

        // If the property doesn't have a name, it has to be an element of an
        // array. In that case, sequence number is the array index.
        *sequenceNumber = (uint32_t)nodeIndex << 1;
        if (dictionaries->annotationDictionary == parentDictionary)
        {
            *sequenceNumber |= 1;
        }
        return 0;
    }

    // If we are here, the property has a name.
    const uint8_t* dictionary =
        bejGetRelatedDictionary(dictionaries, parentDictionary, node->name);
    bool isAnnotation = dictionary == dictionaries->annotationDictionary;
    // If this node's dictionary and its parent's dictionary is different,
    // this node should start searching from the beginning of its
    // dictionary. This should only happen for property annotations of form
    // property@annotation_class.annotation_name.
    if (dictionary != parentDictionary)
    {
        // Redundancy check.
        if (!isAnnotation)
        {
            fprintf(stderr,
                    "Dictionary for property %s should be the annotation "
                    "dictionary. Might be a encoding failure. Maybe the "
                    "JSON tree is not created correctly.",
                    node->name);
            return -1;
        }
        dictStartingOffset = bejDictGetFirstAnnotatedPropertyOffset();
    }

    const struct BejDictionaryProperty* property;
    int ret = bejDictGetPropertyByName(dictionary, dictStartingOffset,
                                       node->name, &property, NULL);
    if (ret != 0)
    {
        fprintf(stderr,
                "Failed to find dictionary entry for name %s. Search started "
                "at offset: %u. ret: %d\n",
                node->name, dictStartingOffset, ret);
        return ret;
    }

    if (nodeDictionary != NULL)
    {
        *nodeDictionary = dictionary;
    }

    if (childEntryOffset != NULL)
    {
        *childEntryOffset = property->childPointerOffset;
    }

    *sequenceNumber = (uint32_t)(property->sequenceNumber) << 1;
    if (isAnnotation)
    {
        *sequenceNumber |= 1;
    }
    return 0;
}

/**
 * @brief Update metadata of leaf nodes.
 *
 * @param dictionaries - dictionaries needed for encoding.
 * @param parentDictionary - dictionary used by this node's parent.
 * @param childP - a pointer to the leaf node.
 * @param childIndex - if this node is an array element, this is the array
 * index.
 * @param dictStartingOffset - starting dictionary child offset value of this
 * node's parent.
 * @return 0 if successful.
 */
static int bejUpdateLeafNodeMetaData(const struct BejDictionaries* dictionaries,
                                     const uint8_t* parentDictionary,
                                     void* childP, uint16_t childIndex,
                                     uint16_t dictStartingOffset)
{
    // TODO: Implement this
    (void)dictionaries;
    (void)parentDictionary;
    (void)childIndex;
    (void)dictStartingOffset;

    struct RedfishPropertyLeaf* chNode = childP;
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
    const uint8_t* nodeDictionary;
    uint16_t childEntryOffset;
    uint32_t sequenceNumber;

    // Get the dictionary related data from the node.
    RETURN_IF_IERROR(bejFindSeqNumAndChildDictOffset(
        dictionaries, parentDictionary, &node->nodeAttr, nodeIndex,
        dictStartingOffset, &sequenceNumber, &nodeDictionary,
        &childEntryOffset));

    node->metaData.sequenceNumber = sequenceNumber;
    node->metaData.childrenDictPropOffset = childEntryOffset;
    node->metaData.nextChild = node->firstChild;
    node->metaData.nextChildIndex = 0;
    node->metaData.dictionary = nodeDictionary;
    node->metaData.vSize = 0;

    // S: Size needed for encoding sequence number.
    node->metaData.sflSize = bejNnintSizeOfValue(node->metaData.sequenceNumber);
    // F: Size of the format byte is 1.
    node->metaData.sflSize += 1;
    // V: Only for bejArray and bejSet types, value size should include the
    // children count. We need to add the size needs to encode all the children
    // later.
    if (node->nodeAttr.format.principalDataType != bejPropertyAnnotation)
    {
        node->metaData.vSize = bejNnintSizeOfValue(node->nChildren);
    }
    return 0;
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
    void* childP = parent->metaData.nextChild;

    // Process all the children belongs to the parent.
    while (childP != NULL)
    {
        // If we find a child with its own child nodes, add it to the stack and
        // return.
        if (bejTreeIsParentType(childP))
        {
            RETURN_IF_IERROR(bejUpdateParentMetaData(
                dictionaries, parent->metaData.dictionary,
                parent->metaData.childrenDictPropOffset, childP,
                parent->metaData.nextChildIndex));

            RETURN_IF_IERROR(stack->stackPush(childP, stack->stackContext));
            bejParentGoToNextChild(parent, childP);
            return 0;
        }

        RETURN_IF_IERROR(
            bejUpdateLeafNodeMetaData(dictionaries, parent->metaData.dictionary,
                                      childP, parent->metaData.nextChildIndex,
                                      parent->metaData.childrenDictPropOffset));
        // Use the child value size to update the parent value size.
        struct RedfishPropertyLeaf* leafChild = childP;
        // V: Include the child size in parent's value size.
        parent->metaData.vSize +=
            (leafChild->metaData.sflSize + leafChild->metaData.vSize);

        // Get the next child belongs to the parent.
        childP = bejParentGoToNextChild(parent, childP);
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
        parent->metaData.sflSize += bejNnintSizeOfValue(parent->metaData.vSize);

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
