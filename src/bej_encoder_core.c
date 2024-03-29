#include "bej_encoder_core.h"

#include "bej_common.h"
#include "bej_encoder_metadata.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Encode a unsigned value with nnint format.
 */
static int bejEncodeNnint(uint64_t value,
                          struct BejEncoderOutputHandler* output)
{
    // The length of the value bytes in nnint.
    uint8_t nnintLengthByte = bejNnintLengthFieldOfUInt(value);
    RETURN_IF_IERROR(output->recvOutput(&nnintLengthByte, sizeof(uint8_t),
                                        output->handlerContext));
    // Write the nnint value bytes.
    return output->recvOutput(&value, nnintLengthByte, output->handlerContext);
}

/**
 * @brief Encode a BejTupleF type.
 */
static int bejEncodeFormat(const struct BejTupleF* format,
                           struct BejEncoderOutputHandler* output)
{
    return output->recvOutput(format, sizeof(struct BejTupleF),
                              output->handlerContext);
}

/**
 * @brief Encode a BejSet or BejArray type.
 */
static int bejEncodeBejSetOrArray(struct RedfishPropertyParent* node,
                                  struct BejEncoderOutputHandler* output)
{
    // Encode Sequence number.
    RETURN_IF_IERROR(bejEncodeNnint(node->metaData.sequenceNumber, output));
    // Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->nodeAttr.format, output));
    // Encode the value length.
    RETURN_IF_IERROR(bejEncodeNnint(node->metaData.vSize, output));
    // Encode the child count
    return bejEncodeNnint(node->nChildren, output);
}

/**
 * @brief Encode an integer to bejInteger type.
 */
static uint8_t bejEncodeInteger(int64_t val,
                                struct BejEncoderOutputHandler* output)
{
    uint8_t copyLength = bejIntLengthOfValue(val);
    return output->recvOutput(&val, copyLength, output->handlerContext);
}

/**
 * @brief Encode a BejInteger type.
 */
int bejEncodeBejInteger(struct RedfishPropertyLeafInt* node,
                        struct BejEncoderOutputHandler* output)
{
    // Encode Sequence number.
    RETURN_IF_IERROR(
        bejEncodeNnint(node->leaf.metaData.sequenceNumber, output));
    // Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->leaf.nodeAttr.format, output));
    // Encode the value length.
    RETURN_IF_IERROR(bejEncodeNnint(node->leaf.metaData.vSize, output));
    // Encode the value.
    return bejEncodeInteger(node->value, output);
}

/**
 * @brief Encode a BejEnum type.
 */
int bejEncodeBejEnum(struct RedfishPropertyLeafEnum* node,
                     struct BejEncoderOutputHandler* output)
{
    // S: Encode Sequence number.
    RETURN_IF_IERROR(
        bejEncodeNnint(node->leaf.metaData.sequenceNumber, output));
    // F: Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->leaf.nodeAttr.format, output));
    // L: Encode the value length.
    RETURN_IF_IERROR(bejEncodeNnint(node->leaf.metaData.vSize, output));
    // V: Encode the value.
    return bejEncodeNnint(node->enumValueSeq, output);
}

int bejEncodeBejString(struct RedfishPropertyLeafString* node,
                       struct BejEncoderOutputHandler* output)
{
    // S: Encode Sequence number.
    RETURN_IF_IERROR(
        bejEncodeNnint(node->leaf.metaData.sequenceNumber, output));
    // F: Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->leaf.nodeAttr.format, output));
    // L: Encode the value length.
    RETURN_IF_IERROR(bejEncodeNnint(node->leaf.metaData.vSize, output));
    // V: Encode the value.
    return output->recvOutput((void*)node->value, node->leaf.metaData.vSize,
                              output->handlerContext);
}

int bejEncodeBejReal(struct RedfishPropertyLeafReal* node,
                     struct BejEncoderOutputHandler* output)
{
    // S: Encode Sequence number.
    RETURN_IF_IERROR(
        bejEncodeNnint(node->leaf.metaData.sequenceNumber, output));
    // F: Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->leaf.nodeAttr.format, output));
    // L: Encode the value length.
    RETURN_IF_IERROR(bejEncodeNnint(node->leaf.metaData.vSize, output));
    // V: Encode the value.
    // Length of the "whole" value as nnint.
    RETURN_IF_IERROR(
        bejEncodeNnint(bejIntLengthOfValue(node->bejReal.whole), output));
    // Add the "whole" value.
    RETURN_IF_IERROR(bejEncodeInteger(node->bejReal.whole, output));
    // Leading zero count as a nnint.
    RETURN_IF_IERROR(bejEncodeNnint(node->bejReal.zeroCount, output));
    // Fraction as a nnint.
    RETURN_IF_IERROR(bejEncodeNnint(node->bejReal.fract, output));
    // Exp length as a nnint.
    RETURN_IF_IERROR(bejEncodeNnint(node->bejReal.expLen, output));
    if (node->bejReal.expLen > 0)
    {
        // Exp length as a nnint.
        RETURN_IF_IERROR(bejEncodeNnint(node->bejReal.expLen, output));
        RETURN_IF_IERROR(bejEncodeInteger(node->bejReal.exp, output));
    }
    return 0;
}

int bejEncodeBejBool(struct RedfishPropertyLeafBool* node,
                     struct BejEncoderOutputHandler* output)
{
    // S: Encode Sequence number.
    RETURN_IF_IERROR(
        bejEncodeNnint(node->leaf.metaData.sequenceNumber, output));
    // F: Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->leaf.nodeAttr.format, output));
    // L: Encode the value length.
    RETURN_IF_IERROR(bejEncodeNnint(node->leaf.metaData.vSize, output));
    // V: Encode the value.
    uint8_t value = node->value ? 0xFF : 0x00;
    return output->recvOutput(&value, /*data_size=*/sizeof(uint8_t),
                              output->handlerContext);
}

int bejEncodeBejProAnno(struct RedfishPropertyParent* node,
                        struct BejEncoderOutputHandler* output)
{
    // Encode Sequence number.
    RETURN_IF_IERROR(bejEncodeNnint(node->metaData.sequenceNumber, output));
    // Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->nodeAttr.format, output));
    // Encode the value length.
    return bejEncodeNnint(node->metaData.vSize, output);
}

/**
 * @brief Encode a BejNull type.
 */
int bejEncodeBejNull(struct RedfishPropertyLeafNull* node,
                     struct BejEncoderOutputHandler* output)
{
    // S: Encode Sequence number.
    RETURN_IF_IERROR(
        bejEncodeNnint(node->leaf.metaData.sequenceNumber, output));
    // F: Add the format.
    RETURN_IF_IERROR(bejEncodeFormat(&node->leaf.nodeAttr.format, output));
    // L: Encode the value length.
    return bejEncodeNnint(node->leaf.metaData.vSize, output);
}

/**
 * @brief Encode the provided node.
 */
static int bejEncodeNode(void* node, struct BejEncoderOutputHandler* output)
{
    struct RedfishPropertyNode* nodeInfo = node;
    switch (nodeInfo->format.principalDataType)
    {
        case bejSet:
            RETURN_IF_IERROR(bejEncodeBejSetOrArray(node, output));
            break;
        case bejArray:
            RETURN_IF_IERROR(bejEncodeBejSetOrArray(node, output));
            break;
        case bejNull:
            RETURN_IF_IERROR(bejEncodeBejNull(node, output));
            break;
        case bejInteger:
            RETURN_IF_IERROR(bejEncodeBejInteger(node, output));
            break;
        case bejEnum:
            RETURN_IF_IERROR(bejEncodeBejEnum(node, output));
            break;
        case bejString:
            RETURN_IF_IERROR(bejEncodeBejString(node, output));
            break;
        case bejReal:
            RETURN_IF_IERROR(bejEncodeBejReal(node, output));
            break;
        case bejBoolean:
            RETURN_IF_IERROR(bejEncodeBejBool(node, output));
            break;
        case bejPropertyAnnotation:
            RETURN_IF_IERROR(bejEncodeBejProAnno(node, output));
            break;
        default:
            fprintf(stderr, "Unsupported node type: %d\n",
                    nodeInfo->format.principalDataType);
            return -1;
    }
    return 0;
}

/**
 * @brief A helper function to add a parent to the stack.
 */
static int bejPushParentToStack(struct RedfishPropertyParent* parent,
                                struct BejPointerStackCallback* stack)
{
    // Before pushing the parent node, initialize its nextChild as the first
    // child.
    parent->metaData.nextChild = parent->firstChild;
    return stack->stackPush(parent, stack->stackContext);
}

/**
 * @brief Process all the child nodes of a parent.
 */
static int bejProcessChildNodes(struct RedfishPropertyParent* parent,
                                struct BejPointerStackCallback* stack,
                                struct BejEncoderOutputHandler* output)
{
    // Get the next child of the parent.
    void* childPtr = parent->metaData.nextChild;

    while (childPtr != NULL)
    {
        // First encode the current child node.
        RETURN_IF_IERROR(bejEncodeNode(childPtr, output));
        // If this child node has its own children, add it to the stack and
        // return. Because we need to encode the children of the newly added
        // node before continuing to encode the child nodes of the current
        // parent.
        if (bejTreeIsParentType(childPtr))
        {
            RETURN_IF_IERROR(bejPushParentToStack(childPtr, stack));
            // Update the next child of the current parent we need to
            // process.
            bejParentGoToNextChild(parent, childPtr);
            return 0;
        }
        childPtr = bejParentGoToNextChild(parent, childPtr);
    }
    return 0;
}

/**
 * @brief Encode the provided JSON tree.
 *
 * The node metadata should be initialized before using this function.
 */
static int bejEncodeTree(struct RedfishPropertyParent* root,
                         struct BejPointerStackCallback* stack,
                         struct BejEncoderOutputHandler* output)
{
    // We need to encode a parent node before its child nodes. So encoding the
    // root first.
    RETURN_IF_IERROR(bejEncodeNode(root, output));
    // Once the root is encoded, push it to the stack used to traverse the child
    // nodes. We need to keep a parent in this stack until all the child nodes
    // of this parent has been encoded. Only then we remove the parent node from
    // the stack.
    RETURN_IF_IERROR(bejPushParentToStack(root, stack));

    while (!stack->stackEmpty(stack->stackContext))
    {
        struct RedfishPropertyParent* parent =
            stack->stackPeek(stack->stackContext);

        // Encode all the child nodes of the current parent node. If one of
        // these child nodes has its own child nodes, that child node will be
        // encoded and added to the stack and this function will return. The
        // rest of the children of the current parent will be encoded later
        // (after processing all the nodes under the child node added to the
        // stack).
        RETURN_IF_IERROR(bejProcessChildNodes(parent, stack, output));

        // If a new node hasn't been added to the stack by
        // bejProcessChildNodes(), we know that this parent's child nodes have
        // been processed. If a new node has been added, then next we need to
        // process the children of the newly added node.
        if (parent != stack->stackPeek(stack->stackContext))
        {
            continue;
        }
        stack->stackPop(stack->stackContext);
    }
    return 0;
}

int bejEncode(const struct BejDictionaries* dictionaries,
              uint16_t majorSchemaStartingOffset,
              enum BejSchemaClass schemaClass,
              struct RedfishPropertyParent* root,
              struct BejEncoderOutputHandler* output,
              struct BejPointerStackCallback* stack)
{
    NULL_CHECK(dictionaries, "dictionaries");
    NULL_CHECK(dictionaries->schemaDictionary, "schemaDictionary");
    NULL_CHECK(dictionaries->annotationDictionary, "annotationDictionary");

    NULL_CHECK(root, "root");

    NULL_CHECK(output, "output");
    NULL_CHECK(stack, "stack");

    // Assert root node.
    if (root->nodeAttr.format.principalDataType != bejSet)
    {
        fprintf(stderr, "Invalid root node\n");
        return -1;
    }

    // First we need to encode a parent node before its child nodes. But before
    // encoding the parent node, the encoder has to figure out the total size
    // need to encode the parent's child nodes. Therefore first the encoder need
    // to visit the child nodes and calculate the size need to encode them
    // before producing the encoded bytes for the parent node.
    //
    // So first the encoder will visit child nodes and calculate the size need
    // to encode each child node. Then store this information in metadata
    // properties in each node struct.
    // Next the encoder will again visit each node starting from the parent
    // node, and produce the encoded bytes.

    // First calculate metadata for encoding each node.
    RETURN_IF_IERROR(bejUpdateNodeMetadata(
        dictionaries, majorSchemaStartingOffset, root, stack));

    // Derive the header of the encoded output.
    // BEJ version
    uint32_t version = BEJ_VERSION;
    RETURN_IF_IERROR(
        output->recvOutput(&version, sizeof(uint32_t), output->handlerContext));
    uint16_t reserved = 0;
    RETURN_IF_IERROR(output->recvOutput(&reserved, sizeof(uint16_t),
                                        output->handlerContext));
    RETURN_IF_IERROR(output->recvOutput(&schemaClass, sizeof(uint8_t),
                                        output->handlerContext));

    // Produce the encoded bytes for the nodes using the previously calculated
    // metadata.
    return bejEncodeTree(root, stack, output);
}
