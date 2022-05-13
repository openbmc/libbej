#include "bej_decoder_core.h"

#include "bej_dictionary.h"
#include "stdio.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

// TODO: Support nested annotations for version 0xF1F1F000
const uint32_t supportedBejVersions[] = {0xF1F0F000};

/**
 * @brief Call a callback function. If the callback function is NULL, this will
 * not do anything. If the callback function returns a non-zero value, this will
 * cause the caller to return with the non-zero status.
 */
#define RETURN_IF_CALLBACK_IERROR(function, ...)                               \
    do                                                                         \
    {                                                                          \
        if ((function) != NULL)                                                \
        {                                                                      \
            int __status = ((function)(__VA_ARGS__));                          \
            if (__status != 0)                                                 \
            {                                                                  \
                return __status;                                               \
            }                                                                  \
        }                                                                      \
    } while (0)

/**
 * @brief Get offsets of SFLV fields with respect to the enSegment start.
 *
 * @param enSegment - a valid pointer to a start of a SFLV bejTuple.
 * @param offsets - this will hold the local offsets.
 */
static void bejGetLocalBejSFLVOffsets(const uint8_t* enSegment,
                                      struct BejSFLVOffset* offsets)
{
    // Structure of the SFLV.
    //   [Number of bytes need to represent the sequence number] - uint8_t
    //   [SequenceNumber] - multi byte
    //   [Format] - uint8_t
    //   [Number of bytes need to represent the value length] - uint8_t
    //   [Value length] - multi byte

    // Number of bytes need to represent the sequence number.
    const uint8_t seqSize = *enSegment;
    // Start of format.
    const uint32_t formatOffset = sizeof(uint8_t) + seqSize;
    // Start of length of the value-length bytes.
    const uint32_t valueLenNnintOffset = formatOffset + sizeof(uint8_t);
    // Number of bytes need to represent the value length.
    const uint8_t valueLengthSize = *(enSegment + valueLenNnintOffset);
    // Start of the Value.
    const uint32_t valueOffset =
        valueLenNnintOffset + sizeof(uint8_t) + valueLengthSize;

    offsets->formatOffset = formatOffset;
    offsets->valueLenNnintOffset = valueLenNnintOffset;
    offsets->valueOffset = valueOffset;
}

/**
 * @brief Initialize sflv struct in params struct.
 *
 * @param params - a valid BejHandleTypeFuncParam struct with
 * params->state.encodedSubStream pointing to the start of the encoded stream
 * and params->state.encodedStreamOffset pointing to the current bejTuple.
 */
static void bejInitSFLVStruct(struct BejHandleTypeFuncParam* params)
{
    struct BejSFLVOffset localOffset;
    // Get offsets of different SFLV fields with respect to start of the encoded
    // segment.
    bejGetLocalBejSFLVOffsets(params->state.encodedSubStream, &localOffset);
    struct BejSFLV* sflv = &params->sflv;
    const uint32_t valueLength = (uint32_t)(rdeGetNnint(
        params->state.encodedSubStream + localOffset.valueLenNnintOffset));
    // Sequence number itself should be 16bits. Using 32bits for
    // [sequence_number + schema_type].
    uint32_t tupleS = (uint32_t)(rdeGetNnint(params->state.encodedSubStream));
    sflv->tupleS.schema = (uint8_t)(tupleS & DICTIONARY_TYPE_MASK);
    sflv->tupleS.sequenceNumber =
        (uint16_t)((tupleS & (~DICTIONARY_TYPE_MASK)) >>
                   DICTIONARY_SEQ_NUM_SHIFT);
    sflv->format = *(struct BejTupleF*)(params->state.encodedSubStream +
                                        localOffset.formatOffset);
    sflv->valueLength = valueLength;
    sflv->valueEndOffset = params->state.encodedStreamOffset +
                           localOffset.valueOffset + valueLength;
    sflv->value = params->state.encodedSubStream + localOffset.valueOffset;
}

/**
 * @brief Decodes an encoded bej stream.
 *
 * @param schemaDictionary - main schema dictionary to use.
 * @param annotationDictionary - annotation dictionary
 * @param enStream - encoded stream without the PLDM header.
 * @param streamLen - length of the enStream.
 * @param stackCallback - callbacks for stack handlers.
 * @param decodedCallback - callbacks for extracting decoded properties.
 * @param callbacksDataPtr - data pointer to pass to decoded callbacks. This can
 * be used pass additional data.
 * @param stackDataPtr - data pointer to pass to stack callbacks. This can be
 * used pass additional data.
 * @return 0 if successful.
 */
static int bejDecode(const uint8_t* schemaDictionary,
                     const uint8_t* annotationDictionary,
                     const uint8_t* enStream, uint32_t streamLen,
                     const struct BejStackCallback* stackCallback,
                     const struct BejDecodedCallback* decodedCallback,
                     void* callbacksDataPtr, void* stackDataPtr)
{
    struct BejHandleTypeFuncParam params = {0};

    params.stackCallback = stackCallback;
    params.decodedCallback = decodedCallback;
    params.callbacksDataPtr = callbacksDataPtr;
    params.stackDataPtr = stackDataPtr;

    // We only add names of set properties. We don't use names for array
    // properties. Here we are omitting the name of the root set.
    params.state.addPropertyName = false;
    // Current location of the encoded segment we are processing.
    params.state.encodedStreamOffset = 0;
    // At start, parent property from the main dictionary is the first property.
    params.state.mainDictPropOffset = bejDictGetPropertyHeadOffset();
    params.state.annoDictPropOffset = bejDictGetFirstAnnotatedPropertyOffset();

    params.annotDictionary = annotationDictionary;
    params.mainDictionary = schemaDictionary;

    while (params.state.encodedStreamOffset < streamLen)
    {
        // Go to the next encoded segment in the encoded stream.
        params.state.encodedSubStream =
            enStream + params.state.encodedStreamOffset;
        bejInitSFLVStruct(&params);

        if (params.sflv.format.readOnlyProperty)
        {
            RETURN_IF_CALLBACK_IERROR(
                params.decodedCallback->callbackReadonlyProperty,
                params.sflv.tupleS.sequenceNumber, params.callbacksDataPtr);
        }

        // TODO: Handle nullable property types. These are indicated by
        // params.sflv.format.nullableProperty
        switch (params.sflv.format.principalDataType)
        {
            case BejSet:
                // TODO: Add support for BejSet decoding.
                fprintf(stderr, "No BejSet support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejArray:
                // TODO: Add support for BejArray decoding.
                fprintf(stderr, "No BejArray support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejNull:
                // TODO: Add support for BejNull decoding.
                fprintf(stderr, "No BejNull support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejInteger:
                // TODO: Add support for BejInteger decoding.
                fprintf(stderr, "No BejInteger support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejEnum:
                // TODO: Add support for BejEnum decoding.
                fprintf(stderr, "No BejEnum support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejString:
                // TODO: Add support for BejString decoding.
                fprintf(stderr, "No BejString support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejReal:
                // TODO: Add support for BejReal decoding.
                fprintf(stderr, "No BejReal support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejBoolean:
                // TODO: Add support for BejBoolean decoding.
                fprintf(stderr, "No BejBoolean support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejBytestring:
                // TODO: Add support for BejBytestring decoding.
                fprintf(stderr, "No BejBytestring support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejChoice:
                // TODO: Add support for BejChoice decoding.
                fprintf(stderr, "No BejChoice support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejPropertyAnnotation:
                // TODO: Add support for BejPropertyAnnotation decoding.
                fprintf(stderr, "No BejPropertyAnnotation support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejResourceLink:
                // TODO: Add support for BejResourceLink decoding.
                fprintf(stderr, "No BejResourceLink support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case BejResourceLinkExpansion:
                // TODO: Add support for BejResourceLinkExpansion decoding.
                fprintf(stderr, "No BejResourceLinkExpansion support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            default:
                break;
        };
    };
    // TODO: Enable this once we are handling different data types.
    // RETURN_IF_IERROR(bejProcessEnding(&params, /*canBeEmpty=*/true));
    if (!params.stackCallback->stackEmpty(params.stackDataPtr))
    {
        fprintf(stderr, "Ending stack should be empty but its not. Something "
                        "must have gone wrong with the encoding\n");
        return BejErrorUnknown;
    }
    return 0;
}

/**
 * @brief Check if a bej version is supported by this decoder
 *
 * @param bejVersion - the bej version in the received encoded stream
 * @return 0 if supported.
 */
static int bejIsSupported(uint32_t bejVersion)
{
    for (uint32_t i = 0; i < sizeof(supportedBejVersions) / sizeof(uint32_t);
         ++i)
    {
        if (bejVersion == supportedBejVersions[i])
        {
            return 0;
        }
    }
    fprintf(stderr, "Bej decoder doesn't support the bej version: %u\n",
            bejVersion);
    return BejErrorNotSuppoted;
}

int bejDecodePldmBlock(const struct BejDictionaries* dictionaries,
                       const uint8_t* encodedPldmBlock, uint32_t blockLength,
                       const struct BejStackCallback* stackCallback,
                       const struct BejDecodedCallback* decodedCallback,
                       void* callbacksDataPtr, void* stackDataPtr)
{
    uint32_t pldmHeaderSize = sizeof(struct BejPldmBlockHeader);
    if (blockLength < pldmHeaderSize)
    {
        fprintf(stderr, "Invalid pldm block size: %u\n", blockLength);
        return BejErrorInvalidSize;
    }

    const struct BejPldmBlockHeader* pldmHeader =
        (const struct BejPldmBlockHeader*)encodedPldmBlock;
    RETURN_IF_IERROR(bejIsSupported(pldmHeader->bejVersion));
    if (pldmHeader->schemaClass == BejAnnotationSchemaClass)
    {
        fprintf(stderr,
                "Encoder schema class cannot be BejAnnotationSchemaClass\n");
        return BejErrorNotSuppoted;
    }
    // TODO: Add support for CollectionMemberType schema class.
    if (pldmHeader->schemaClass == BejCollectionMemberTypeSchemaClass)
    {
        fprintf(stderr, "Decoder doesn't support "
                        "BejCollectionMemberTypeSchemaClass yet.\n");
        return BejErrorNotSuppoted;
    }
    // TODO: Add support for Error schema class.
    if (pldmHeader->schemaClass == BejErrorSchemaClass)
    {
        fprintf(stderr, "Decoder doesn't support BejErrorSchemaClass yet.\n");
        return BejErrorNotSuppoted;
    }

    // Skip the PLDM header.
    const uint8_t* enStream = encodedPldmBlock + pldmHeaderSize;
    uint32_t streamLen = blockLength - pldmHeaderSize;
    return bejDecode(dictionaries->schemaDictionary,
                     dictionaries->annotationDictionary, enStream, streamLen,
                     stackCallback, decodedCallback, callbacksDataPtr,
                     stackDataPtr);
}