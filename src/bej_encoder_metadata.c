#include "bej_encoder_metadata.h"

#include "bej_common.h"
#include "bej_dictionary.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Maximum digits supported in the fractional part of a real number.
 */
#define BEJ_REAL_PRECISION 16

/**
 * @brief bejTupleL size of an integer.
 *
 * Maximum bytes possible for an integer is 8. Therefore to encode the length of
 * an integer using a nnint, we only need two bytes. [byte1: nnint length,
 * byte2: integer length [0-8]]
 */
#define BEJ_TUPLE_L_SIZE_FOR_BEJ_INTEGER 2

/**
 * @brief bejTupleL size of a bool.
 *
 * 1byte for the nnint length and 1 byte for the value.
 */
#define BEJ_TUPLE_L_SIZE_FOR_BEJ_BOOL 2

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

static int bejUpdateIntMetaData(const struct BejDictionaries* dictionaries,
                                const uint8_t* parentDictionary,
                                struct RedfishPropertyLeafInt* node,
                                uint16_t nodeIndex, uint16_t dictStartingOffset)
{
    uint32_t sequenceNumber;
    RETURN_IF_IERROR(bejFindSeqNumAndChildDictOffset(
        dictionaries, parentDictionary, &node->leaf.nodeAttr, nodeIndex,
        dictStartingOffset, &sequenceNumber, NULL, NULL));
    node->leaf.metaData.sequenceNumber = sequenceNumber;

    // Calculate the size for encoding this in a SFLV tuple.
    // S: Size needed for encoding sequence number.
    node->leaf.metaData.sflSize = bejNnintEncodingSizeOfUInt(sequenceNumber);
    // F: Size of the format byte is 1.
    node->leaf.metaData.sflSize += 1;
    // L: Length needed for the value.
    node->leaf.metaData.sflSize += BEJ_TUPLE_L_SIZE_FOR_BEJ_INTEGER;
    // V: Bytes used for the value.
    node->leaf.metaData.vSize = bejIntLengthOfValue(node->value);
    return 0;
}

static int bejUpdateStringMetaData(const struct BejDictionaries* dictionaries,
                                   const uint8_t* parentDictionary,
                                   struct RedfishPropertyLeafString* node,
                                   uint16_t nodeIndex,
                                   uint16_t dictStartingOffset)
{
    uint32_t sequenceNumber;
    RETURN_IF_IERROR(bejFindSeqNumAndChildDictOffset(
        dictionaries, parentDictionary, &(node->leaf.nodeAttr), nodeIndex,
        dictStartingOffset, &sequenceNumber, NULL, NULL));
    node->leaf.metaData.sequenceNumber = sequenceNumber;

    // Calculate the size for encoding this in a SFLV tuple.
    // S: Size needed for encoding sequence number.
    node->leaf.metaData.sflSize = bejNnintEncodingSizeOfUInt(sequenceNumber);
    // F: Size of the format byte is 1.
    node->leaf.metaData.sflSize += 1;
    // L: Length needed for the string including the NULL character. Length is
    // in nnint format.
    size_t strLenWithNull = strlen(node->value) + 1;
    node->leaf.metaData.sflSize += bejNnintEncodingSizeOfUInt(strLenWithNull);
    // V: Bytes used for the value.
    node->leaf.metaData.vSize = strLenWithNull;
    return 0;
}

static int bejUpdateRealMetaData(const struct BejDictionaries* dictionaries,
                                 const uint8_t* parentDictionary,
                                 struct RedfishPropertyLeafReal* node,
                                 uint16_t nodeIndex,
                                 uint16_t dictStartingOffset)
{
    uint32_t sequenceNumber;
    RETURN_IF_IERROR(bejFindSeqNumAndChildDictOffset(
        dictionaries, parentDictionary, &(node->leaf.nodeAttr), nodeIndex,
        dictStartingOffset, &sequenceNumber, NULL, NULL));
    node->leaf.metaData.sequenceNumber = sequenceNumber;

    if (node->value > (double)INT64_MAX)
    {
        // TODO: We should use the exponent.
        fprintf(
            stderr,
            "Need to add support to encode double value larger than INT64_MAX\n");
        return -1;
    }

    // Calculate the size for encoding this in a SFLV tuple.
    // S: Size needed for encoding sequence number.
    node->leaf.metaData.sflSize = bejNnintEncodingSizeOfUInt(sequenceNumber);
    // F: Size of the format byte is 1.
    node->leaf.metaData.sflSize += 1;
    // We need to breakdown the real number to bejReal type to determine the
    // length. We are not gonna add an exponent. It will only be the whole part
    // and the fraction part. Get the whole part
    double originalWhole;
    double originalFract = modf(node->value, &originalWhole);

    // Convert the fraction to a whole value for encoding.
    // Create a new value by multiplying the original fraction by 10. Do this
    // until the fraction of the new value is 0 or we reach the precision. Eg
    // 0.00105: This fraction value has two leading zeros. We will keep
    // multiplying this by 10 until the fraction of the result of that
    // multiplication is 0.
    double originalFactConvertedToWhole = fabs(originalFract);
    double fract = originalFract;
    double intPart;
    uint32_t leadingZeros = 0;
    uint32_t precision = 0;
    while (fract != 0 && precision < BEJ_REAL_PRECISION)
    {
        originalFactConvertedToWhole = originalFactConvertedToWhole * 10;
        fract = modf(originalFactConvertedToWhole, &intPart);
        // If the integer portion is 0, that means we still have leading zeros.
        if (intPart == 0)
        {
            ++leadingZeros;
        }
        ++precision;
    }
    node->bejReal.whole = (int64_t)originalWhole;
    node->bejReal.zeroCount = leadingZeros;
    node->bejReal.fract = (int64_t)originalFactConvertedToWhole;
    // We are omitting exp. So the exp length should be 0.
    node->bejReal.expLen = 0;
    node->bejReal.exp = 0;

    // Calculate the sizes needed for storing bejReal fields.
    // nnint for the length of the "whole" value.
    node->leaf.metaData.vSize = BEJ_TUPLE_L_SIZE_FOR_BEJ_INTEGER;
    // Length needed for the "whole" value.
    node->leaf.metaData.vSize += bejIntLengthOfValue((int64_t)originalWhole);
    // nnint for leading zero count.
    node->leaf.metaData.vSize += bejNnintEncodingSizeOfUInt(leadingZeros);
    // nnint for the factional part.
    node->leaf.metaData.vSize +=
        bejNnintEncodingSizeOfUInt((int64_t)originalFactConvertedToWhole);
    // nnint for the exp length. We are omitting exp. So the exp length should
    // be 0.
    node->leaf.metaData.vSize += bejNnintEncodingSizeOfUInt(0);

    // L: nnint for the size needed for encoding the bejReal value.
    node->leaf.metaData.sflSize +=
        bejNnintEncodingSizeOfUInt(node->leaf.metaData.vSize);
    return 0;
}

static int bejUpdateEnumMetaData(const struct BejDictionaries* dictionaries,
                                 const uint8_t* parentDictionary,
                                 struct RedfishPropertyLeafEnum* node,
                                 uint16_t nodeIndex,
                                 uint16_t dictStartingOffset)
{
    const uint8_t* nodeDictionary;
    uint16_t childEntryOffset;
    uint32_t sequenceNumber;
    // If the enum property doesn't have a name, this will simply return the
    // nodeIndex encoded as the sequence number. If not, this will return the
    // sequence number in the dictionary and the starting dictionary index for
    // the enum values.
    RETURN_IF_IERROR(bejFindSeqNumAndChildDictOffset(
        dictionaries, parentDictionary, &(node->leaf.nodeAttr), nodeIndex,
        dictStartingOffset, &sequenceNumber, &nodeDictionary,
        &childEntryOffset));
    // Update the sequence number of the property.
    node->leaf.metaData.sequenceNumber = sequenceNumber;

    // Get the sequence number for the Enum value.
    if (node->leaf.nodeAttr.name != NULL && node->leaf.nodeAttr.name[0] != '\0')
    {
        dictStartingOffset = childEntryOffset;
    }
    const struct BejDictionaryProperty* enumValueProperty;
    int ret = bejDictGetPropertyByName(nodeDictionary, dictStartingOffset,
                                       node->value, &enumValueProperty, NULL);
    if (ret != 0)
    {
        fprintf(
            stderr,
            "Failed to find dictionary entry for enum value %s. Search started "
            "at offset: %u. ret: %d\n",
            node->value, dictStartingOffset, ret);
        return ret;
    }
    node->enumValueSeq = enumValueProperty->sequenceNumber;

    // Calculate the size for encoding this in a SFLV tuple.
    // S: Size needed for encoding sequence number.
    node->leaf.metaData.sflSize = bejNnintEncodingSizeOfUInt(sequenceNumber);
    // F: Size of the format byte is 1.
    node->leaf.metaData.sflSize += 1;
    // V: Bytes used for the value.
    node->leaf.metaData.vSize =
        bejNnintEncodingSizeOfUInt(enumValueProperty->sequenceNumber);
    // L: Length needed for the value nnint.
    node->leaf.metaData.sflSize +=
        bejNnintEncodingSizeOfUInt(node->leaf.metaData.vSize);
    return 0;
}

static int bejUpdateBoolMetaData(const struct BejDictionaries* dictionaries,
                                 const uint8_t* parentDictionary,
                                 struct RedfishPropertyLeafBool* node,
                                 uint16_t nodeIndex,
                                 uint16_t dictStartingOffset)
{
    uint32_t sequenceNumber;
    RETURN_IF_IERROR(bejFindSeqNumAndChildDictOffset(
        dictionaries, parentDictionary, &node->leaf.nodeAttr, nodeIndex,
        dictStartingOffset, &sequenceNumber, NULL, NULL));
    node->leaf.metaData.sequenceNumber = sequenceNumber;

    // Calculate the size for encoding this in a SFLV tuple.
    // S: Size needed for encoding sequence number.
    node->leaf.metaData.sflSize = bejNnintEncodingSizeOfUInt(sequenceNumber);
    // F: Size of the format byte is 1.
    node->leaf.metaData.sflSize += 1;
    // L: Length needed for the value.
    node->leaf.metaData.sflSize += BEJ_TUPLE_L_SIZE_FOR_BEJ_BOOL;
    // V: Bytes used for the value; 0x00 or 0xFF.
    node->leaf.metaData.vSize = 1;
    return 0;
}

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
    struct RedfishPropertyLeaf* chNode = childPtr;

    switch (chNode->nodeAttr.format.principalDataType)
    {
        case bejInteger:
            RETURN_IF_IERROR(
                bejUpdateIntMetaData(dictionaries, parentDictionary, childPtr,
                                     childIndex, dictStartingOffset));
            break;
        case bejString:
            RETURN_IF_IERROR(bejUpdateStringMetaData(
                dictionaries, parentDictionary, childPtr, childIndex,
                dictStartingOffset));
            break;
        case bejReal:
            RETURN_IF_IERROR(
                bejUpdateRealMetaData(dictionaries, parentDictionary, childPtr,
                                      childIndex, dictStartingOffset));
            break;
        case bejEnum:
            RETURN_IF_IERROR(
                bejUpdateEnumMetaData(dictionaries, parentDictionary, childPtr,
                                      childIndex, dictStartingOffset));
            break;
        case bejBoolean:
            RETURN_IF_IERROR(
                bejUpdateBoolMetaData(dictionaries, parentDictionary, childPtr,
                                      childIndex, dictStartingOffset));
            break;
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
    node->metaData.sflSize =
        bejNnintEncodingSizeOfUInt(node->metaData.sequenceNumber);
    // F: Size of the format byte is 1.
    node->metaData.sflSize += 1;
    // V: Only for bejArray and bejSet types, value size should include the
    // children count. We need to add the size needs to encode all the children
    // later.
    if (node->nodeAttr.format.principalDataType != bejPropertyAnnotation)
    {
        node->metaData.vSize = bejNnintEncodingSizeOfUInt(node->nChildren);
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
