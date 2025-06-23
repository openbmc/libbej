#include "bej_decoder_core.h"

#include "bej_dictionary.h"
#include "stdio.h"

#include <inttypes.h>
#include <stdbool.h>
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
 * @brief Get the integer value from BEJ byte stream.
 *
 * @param[in] bytes - valid pointer to a byte stream in little-endian format.
 * @param[in] numOfBytes - number of bytes belongs to the value. Maximum value
 * supported is 8 bytes.
 * @return signed 64bit representation of the value.
 */
static int64_t bejGetIntegerValue(const uint8_t* bytes, uint8_t numOfBytes)
{
    if (numOfBytes == 0)
    {
        return 0;
    }
    uint64_t value = bejGetUnsignedInteger(bytes, numOfBytes);
    uint8_t bitsInVal = numOfBytes * 8;
    // Since numOfBytes > 0, bitsInVal is non negative.
    uint64_t mask = (uint64_t)1 << (uint8_t)(bitsInVal - 1);
    return (int64_t)((value ^ mask) - mask);
}

/**
 * @brief Get offsets of SFLV fields with respect to the enSegment start.
 *
 * @param[in] enSegment - a valid pointer to a start of a SFLV bejTuple.
 * @param[out] offsets - this will hold the local offsets.
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
 * @param[inout] params - a valid BejHandleTypeFuncParam struct with
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
    const uint32_t valueLength = (uint32_t)(bejGetNnint(
        params->state.encodedSubStream + localOffset.valueLenNnintOffset));
    // Sequence number itself should be 16bits. Using 32bits for
    // [sequence_number + schema_type].
    uint32_t tupleS = (uint32_t)(bejGetNnint(params->state.encodedSubStream));
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
 * @brief Get the offset to the first tuple of a bejArray or bejSet.
 *
 * The first part of the value of a bejArray or a bejSet contains an nnint
 * providing the number of elements/tuples. Offset is with respect to the start
 * of the encoded stream.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return offset with respect to the start of the encoded stream.
 */
static uint32_t bejGetFirstTupleOffset(
    const struct BejHandleTypeFuncParam* params)
{
    struct BejSFLVOffset localOffset;
    // Get the offset of the value with respect to the current encoded segment
    // being decoded.
    bejGetLocalBejSFLVOffsets(params->state.encodedSubStream, &localOffset);
    return params->state.encodedStreamOffset + localOffset.valueOffset +
           bejGetNnintSize(params->sflv.value);
}

/**
 * @brief Get the correct property and the dictionary it belongs to.
 *
 * @param[in] params - a BejHandleTypeFuncParam struct pointing to valid
 * dictionaries.
 * @param[in] schemaType - indicate whether to use the annotation dictionary or
 * the main schema dictionary.
 * @param[in] sequenceNumber - sequence number to use for property search. Not
 * using the params->sflv.tupleS.sequenceNumber from the provided params struct.
 * @param[out] dictionary - if the function is successful, this will point to a
 * valid dictionary to be used.
 * @param[out] prop - if the function is successful, this will point to a valid
 * property in a dictionary.
 * @return 0 if successful.
 */
static int bejGetDictionaryAndProperty(
    const struct BejHandleTypeFuncParam* params, uint8_t schemaType,
    uint32_t sequenceNumber, const uint8_t** dictionary,
    const struct BejDictionaryProperty** prop)
{
    uint16_t dictPropOffset;
    // We need to pick the correct dictionary.
    if (schemaType == bejPrimary)
    {
        *dictionary = params->mainDictionary;
        dictPropOffset = params->state.mainDictPropOffset;
    }
    else if (schemaType == bejAnnotation)
    {
        *dictionary = params->annotDictionary;
        dictPropOffset = params->state.annoDictPropOffset;
    }
    else
    {
        fprintf(stderr, "Failed to select a dictionary. schema type: %u\n",
                schemaType);
        return bejErrorInvalidSchemaType;
    }

    int ret =
        bejDictGetProperty(*dictionary, dictPropOffset, sequenceNumber, prop);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to get dictionary property for offset: %u\n",
                dictPropOffset);
        return ret;
    }
    return 0;
}

/**
 * @brief Find and return the property name of the current encoded segment. If
 * the params->state.addPropertyName is false, this will return an empty string.
 *
 * @param[in] params - a valid populated BejHandleTypeFuncParam.
 * @return 0 if successful.
 */
static const char* bejGetPropName(struct BejHandleTypeFuncParam* params)
{
    const uint8_t* dictionary;
    const struct BejDictionaryProperty* prop;
    if (!params->state.addPropertyName ||
        (bejGetDictionaryAndProperty(params, params->sflv.tupleS.schema,
                                     params->sflv.tupleS.sequenceNumber,
                                     &dictionary, &prop) != 0))
    {
        return "";
    }
    return bejDictGetPropertyName(dictionary, prop->nameOffset,
                                  prop->nameLength);
}

/**
 * @brief Look for section endings.
 *
 * This figures out whether the current encoded segment marks a section
 * ending. If so, this function will update the decoder state and pop the stack
 * used to memorize endings. This function should be called after updating the
 * encodedStreamOffset to the end of decoded SFLV tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam which contains the decoder
 * state.
 * @param[in] canBeEmpty - if true, the stack being empty is not an error. If
 * false, stack cannot be empty.
 * @return 0 if successful.
 */
static int bejProcessEnding(struct BejHandleTypeFuncParam* params,
                            bool canBeEmpty)
{
    if (params->stackCallback->stackEmpty(params->stackDataPtr) && !canBeEmpty)
    {
        // If bejProcessEnding has been called after adding an appropriate JSON
        // property, then stack cannot be empty.
        fprintf(stderr, "Ending stack cannot be empty.\n");
        return bejErrorUnknown;
    }

    while (!params->stackCallback->stackEmpty(params->stackDataPtr))
    {
        const struct BejStackProperty* const ending =
            params->stackCallback->stackPeek(params->stackDataPtr);
        // Check whether the current offset location matches the expected ending
        // offset. If so, we are done with that section.
        if (params->state.encodedStreamOffset == ending->streamEndOffset)
        {
            // Since we are going out of a section, we need to reset the
            // dictionary property offsets to this section's parent property
            // start.
            params->state.mainDictPropOffset = ending->mainDictPropOffset;
            params->state.annoDictPropOffset = ending->annoDictPropOffset;
            params->state.addPropertyName = ending->addPropertyName;

            if (ending->sectionType == bejSectionSet)
            {
                RETURN_IF_CALLBACK_IERROR(
                    params->decodedCallback->callbackSetEnd,
                    params->callbacksDataPtr);
            }
            else if (ending->sectionType == bejSectionArray)
            {
                RETURN_IF_CALLBACK_IERROR(
                    params->decodedCallback->callbackArrayEnd,
                    params->callbacksDataPtr);
            }
            params->stackCallback->stackPop(params->stackDataPtr);
        }
        else
        {
            RETURN_IF_CALLBACK_IERROR(
                params->decodedCallback->callbackPropertyEnd,
                params->callbacksDataPtr);
            // Do not change the parent dictionary property offset since we are
            // still inside the same section.
            return 0;
        }
    }
    return 0;
}

/**
 * @brief Check whether the current encoded segment being decoded is an array
 * element.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return true if the encoded segment is an array element. Else false.
 */
static bool bejIsArrayElement(const struct BejHandleTypeFuncParam* params)
{
    // If the encoded segment enters an array section, we are adding a
    // BejSectionArray to the stack. Therefore if the stack is empty, encoded
    // segment cannot be an array element.
    if (params->stackCallback->stackEmpty(params->stackDataPtr))
    {
        return false;
    }
    const struct BejStackProperty* const ending =
        params->stackCallback->stackPeek(params->stackDataPtr);
    // If the stack top element holds a BejSectionArray, encoded segment is
    // an array element.
    return ending->sectionType == bejSectionArray;
}

/**
 * @brief Decodes a BejSet type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejSet(struct BejHandleTypeFuncParam* params)
{
    uint16_t sequenceNumber = params->sflv.tupleS.sequenceNumber;
    // Check whether this BejSet is an array element or not.
    if (bejIsArrayElement(params))
    {
        // Dictionary only contains an entry for element 0.
        sequenceNumber = 0;
    }
    const uint8_t* dictionary;
    const struct BejDictionaryProperty* prop;
    RETURN_IF_IERROR(
        bejGetDictionaryAndProperty(params, params->sflv.tupleS.schema,
                                    sequenceNumber, &dictionary, &prop));

    const char* propName = "";
    if (params->state.addPropertyName)
    {
        propName = bejDictGetPropertyName(dictionary, prop->nameOffset,
                                          prop->nameLength);
    }

    RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackSetStart,
                              propName, params->callbacksDataPtr);

    // Move the offset to the next SFLV tuple (or end). Make sure that this is
    // called before calling bejProcessEnding.
    params->state.encodedStreamOffset = bejGetFirstTupleOffset(params);

    uint64_t elements = bejGetNnint(params->sflv.value);
    // If its an empty set, we are done here.
    if (elements == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackSetEnd,
                                  params->callbacksDataPtr);
        // Since this is an ending of a property (empty array), we should call
        // bejProcessEnding. Unless the whole JSON object is an empty set (which
        // shouldn't be the case), stack cannot be empty.
        bejProcessEnding(params, /*canBeEmpty=*/false);
        return 0;
    }

    // Update the states for the next encoding segment.
    struct BejStackProperty newEnding = {
        .sectionType = bejSectionSet,
        .addPropertyName = params->state.addPropertyName,
        .mainDictPropOffset = params->state.mainDictPropOffset,
        .annoDictPropOffset = params->state.annoDictPropOffset,
        .streamEndOffset = params->sflv.valueEndOffset,
    };
    RETURN_IF_IERROR(
        params->stackCallback->stackPush(&newEnding, params->stackDataPtr));
    params->state.addPropertyName = true;
    if (params->sflv.tupleS.schema == bejAnnotation)
    {
        // Since this set is an annotated type, we need to advance the
        // annotation dictionary for decoding the next segment.
        params->state.annoDictPropOffset = prop->childPointerOffset;
    }
    else
    {
        params->state.mainDictPropOffset = prop->childPointerOffset;
    }
    return 0;
}

/**
 * @brief Decodes a BejArray type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejArray(struct BejHandleTypeFuncParam* params)
{
    const uint8_t* dictionary;
    const struct BejDictionaryProperty* prop;
    RETURN_IF_IERROR(bejGetDictionaryAndProperty(
        params, params->sflv.tupleS.schema, params->sflv.tupleS.sequenceNumber,
        &dictionary, &prop));

    const char* propName = "";
    if (params->state.addPropertyName)
    {
        propName = bejDictGetPropertyName(dictionary, prop->nameOffset,
                                          prop->nameLength);
    }

    RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackArrayStart,
                              propName, params->callbacksDataPtr);

    // Move the offset to the next SFLV tuple (or end). Make sure that this is
    // called before calling bejProcessEnding.
    params->state.encodedStreamOffset = bejGetFirstTupleOffset(params);

    uint64_t elements = bejGetNnint(params->sflv.value);
    // If its an empty array, we are done here.
    if (elements == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackArrayEnd,
                                  params->callbacksDataPtr);
        // Since this is an ending of a property (empty array), we should call
        // bejProcessEnding. Stack cannot be empty since there should be at
        // least 1 parent in the stack.
        bejProcessEnding(params, /*canBeEmpty=*/false);
        return 0;
    }

    // Update the state for next segment decoding.
    struct BejStackProperty newEnding = {
        .sectionType = bejSectionArray,
        .addPropertyName = params->state.addPropertyName,
        .mainDictPropOffset = params->state.mainDictPropOffset,
        .annoDictPropOffset = params->state.annoDictPropOffset,
        .streamEndOffset = params->sflv.valueEndOffset,
    };
    RETURN_IF_IERROR(
        params->stackCallback->stackPush(&newEnding, params->stackDataPtr));
    // We do not add property names for array elements.
    params->state.addPropertyName = false;
    if (params->sflv.tupleS.schema == bejAnnotation)
    {
        // Since this array is an annotated type, we need to advance the
        // annotation dictionary for decoding the next segment.
        params->state.annoDictPropOffset = prop->childPointerOffset;
    }
    else
    {
        params->state.mainDictPropOffset = prop->childPointerOffset;
    }
    return 0;
}

/**
 * @brief Decodes a BejNull type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejNull(struct BejHandleTypeFuncParam* params)
{
    const char* propName = bejGetPropName(params);
    RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackNull, propName,
                              params->callbacksDataPtr);
    params->state.encodedStreamOffset = params->sflv.valueEndOffset;
    return bejProcessEnding(params, /*canBeEmpty=*/false);
}

/**
 * @brief Decodes a BejInteger type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejInteger(struct BejHandleTypeFuncParam* params)
{
    const char* propName = bejGetPropName(params);

    if (params->sflv.valueLength == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackNull,
                                  propName, params->callbacksDataPtr);
    }
    else
    {
        RETURN_IF_CALLBACK_IERROR(
            params->decodedCallback->callbackInteger, propName,
            bejGetIntegerValue(params->sflv.value, params->sflv.valueLength),
            params->callbacksDataPtr);
    }
    params->state.encodedStreamOffset = params->sflv.valueEndOffset;
    return bejProcessEnding(params, /*canBeEmpty=*/false);
}

/**
 * @brief Decodes a BejEnum type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejEnum(struct BejHandleTypeFuncParam* params)
{
    uint16_t sequenceNumber = params->sflv.tupleS.sequenceNumber;
    if (bejIsArrayElement(params))
    {
        sequenceNumber = 0;
    }
    const uint8_t* dictionary;
    const struct BejDictionaryProperty* prop;
    RETURN_IF_IERROR(
        bejGetDictionaryAndProperty(params, params->sflv.tupleS.schema,
                                    sequenceNumber, &dictionary, &prop));

    const char* propName = "";
    if (params->state.addPropertyName)
    {
        propName = bejDictGetPropertyName(dictionary, prop->nameOffset,
                                          prop->nameLength);
    }

    if (params->sflv.valueLength == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackNull,
                                  propName, params->callbacksDataPtr);
    }
    else
    {
        // Get the string for enum value.
        uint16_t enumValueSequenceN =
            (uint16_t)(bejGetNnint(params->sflv.value));
        const struct BejDictionaryProperty* enumValueProp;
        RETURN_IF_IERROR(
            bejDictGetProperty(dictionary, prop->childPointerOffset,
                               enumValueSequenceN, &enumValueProp));
        const char* enumValueName = bejDictGetPropertyName(
            dictionary, enumValueProp->nameOffset, enumValueProp->nameLength);

        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackEnum,
                                  propName, enumValueName,
                                  params->callbacksDataPtr);
    }
    // Update the offset to point to the next possible SFLV tuple.
    params->state.encodedStreamOffset = params->sflv.valueEndOffset;
    return bejProcessEnding(params, /*canBeEmpty=*/false);
}

/**
 * @brief Decodes a BejString type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejString(struct BejHandleTypeFuncParam* params)
{
    // TODO: Handle deferred bindings.
    const char* propName = bejGetPropName(params);

    if (params->sflv.valueLength == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackNull,
                                  propName, params->callbacksDataPtr);
    }
    else
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackString,
                                  propName, (const char*)(params->sflv.value),
                                  params->callbacksDataPtr);
    }
    params->state.encodedStreamOffset = params->sflv.valueEndOffset;
    return bejProcessEnding(params, /*canBeEmpty=*/false);
}

/**
 * @brief Decodes a BejReal type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejReal(struct BejHandleTypeFuncParam* params)
{
    const char* propName = bejGetPropName(params);

    if (params->sflv.valueLength == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackNull,
                                  propName, params->callbacksDataPtr);
    }
    else
    {
        // Real value has the following format.
        // nnint      - Length of whole
        // bejInteger - whole (includes sign for the overall real number)
        // nnint      - Leading zero count for fract
        // nnint      - fract
        // nnint      - Length of exp
        // bejInteger - exp (includes sign for the exponent)
        uint8_t wholeByteLen = (uint8_t)bejGetNnint(params->sflv.value);
        const uint8_t* wholeBejInt =
            params->sflv.value + bejGetNnintSize(params->sflv.value);
        const uint8_t* fractZeroCountNnint = wholeBejInt + wholeByteLen;
        const uint8_t* fractNnint =
            fractZeroCountNnint + bejGetNnintSize(fractZeroCountNnint);
        const uint8_t* lenExpNnint = fractNnint + bejGetNnintSize(fractNnint);
        const uint8_t* expBejInt = lenExpNnint + bejGetNnintSize(lenExpNnint);

        struct BejReal realValue;
        realValue.whole = bejGetIntegerValue(wholeBejInt, wholeByteLen);
        realValue.zeroCount = bejGetNnint(fractZeroCountNnint);
        realValue.fract = bejGetNnint(fractNnint);
        realValue.expLen = (uint8_t)bejGetNnint(lenExpNnint);
        if (realValue.expLen != 0)
        {
            realValue.exp = bejGetIntegerValue(
                expBejInt, (uint8_t)bejGetNnint(lenExpNnint));
        }
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackReal,
                                  propName, &realValue,
                                  params->callbacksDataPtr);
    }
    params->state.encodedStreamOffset = params->sflv.valueEndOffset;
    return bejProcessEnding(params, /*canBeEmpty=*/false);
}

/**
 * @brief Decodes a BejBoolean type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejBoolean(struct BejHandleTypeFuncParam* params)
{
    const char* propName = bejGetPropName(params);

    if (params->sflv.valueLength == 0)
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackNull,
                                  propName, params->callbacksDataPtr);
    }
    else
    {
        RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackBool,
                                  propName, *(params->sflv.value) > 0,
                                  params->callbacksDataPtr);
    }
    params->state.encodedStreamOffset = params->sflv.valueEndOffset;
    return bejProcessEnding(params, /*canBeEmpty=*/false);
}

/**
 * @brief Decodes a BejPropertyAnnotation type SFLV BEJ tuple.
 *
 * @param[in] params - a valid BejHandleTypeFuncParam struct.
 * @return 0 if successful.
 */
static int bejHandleBejPropertyAnnotation(struct BejHandleTypeFuncParam* params)
{
    // TODO: Handle colon-delimited string values.

    // Property annotation has the form OuterProperty@Annotation. First
    // processing the outer property name.
    const uint8_t* outerDictionary;
    const struct BejDictionaryProperty* outerProp;
    RETURN_IF_IERROR(bejGetDictionaryAndProperty(
        params, params->sflv.tupleS.schema, params->sflv.tupleS.sequenceNumber,
        &outerDictionary, &outerProp));

    const char* propName = bejDictGetPropertyName(
        outerDictionary, outerProp->nameOffset, outerProp->nameLength);
    RETURN_IF_CALLBACK_IERROR(params->decodedCallback->callbackAnnotation,
                              propName, params->callbacksDataPtr);

    // Mark the ending of the property annotation.
    struct BejStackProperty newEnding = {
        .sectionType = bejSectionNoType,
        .addPropertyName = params->state.addPropertyName,
        .mainDictPropOffset = params->state.mainDictPropOffset,
        .annoDictPropOffset = params->state.annoDictPropOffset,
        .streamEndOffset = params->sflv.valueEndOffset,
    };
    // Update the states for the next encoding segment.
    RETURN_IF_IERROR(
        params->stackCallback->stackPush(&newEnding, params->stackDataPtr));
    params->state.addPropertyName = true;
    // We might have to change this for nested annotations.
    params->state.mainDictPropOffset = outerProp->childPointerOffset;
    // Point to the start of the value for next decoding.
    params->state.encodedStreamOffset =
        params->sflv.valueEndOffset - params->sflv.valueLength;
    return 0;
}

/**
 * @brief Decodes an encoded bej stream.
 *
 * @param[in] schemaDictionary - main schema dictionary to use.
 * @param[in] annotationDictionary - annotation dictionary
 * @param[in] enStream - encoded stream without the PLDM header.
 * @param[in] streamLen - length of the enStream.
 * @param[in] stackCallback - callbacks for stack handlers.
 * @param[in] decodedCallback - callbacks for extracting decoded properties.
 * @param[in] callbacksDataPtr - data pointer to pass to decoded callbacks. This
 * can be used pass additional data.
 * @param[in] stackDataPtr - data pointer to pass to stack callbacks. This can
 * be used pass additional data.
 *
 * @return 0 if successful.
 */
static int bejDecode(const uint8_t* schemaDictionary,
                     const uint8_t* annotationDictionary,
                     const uint8_t* enStream, uint32_t streamLen,
                     const struct BejStackCallback* stackCallback,
                     const struct BejDecodedCallback* decodedCallback,
                     void* callbacksDataPtr, void* stackDataPtr)
{
    struct BejHandleTypeFuncParam params = {
        .state =
            {
                // We only add names of set properties. We don't use names for
                // array
                // properties. Here we are omitting the name of the root set.
                .addPropertyName = false,
                // At start, parent property from the main dictionary is the
                // first property.
                .mainDictPropOffset = bejDictGetPropertyHeadOffset(),
                .annoDictPropOffset = bejDictGetFirstAnnotatedPropertyOffset(),
                // Current location of the encoded segment we are processing.
                .encodedStreamOffset = 0,
                .encodedSubStream = enStream,
            },
        .mainDictionary = schemaDictionary,
        .annotDictionary = annotationDictionary,
        .decodedCallback = decodedCallback,
        .stackCallback = stackCallback,
        .callbacksDataPtr = callbacksDataPtr,
        .stackDataPtr = stackDataPtr,
    };

    uint64_t maxOperations = 1000000;
    uint64_t operationCount = 0;

    while (params.state.encodedStreamOffset < streamLen)
    {
        if (++operationCount > maxOperations)
        {
            fprintf(stderr, "BEJ decoding exceeded max operations\n");
            return bejErrorNotSuppoted;
        }
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
            case bejSet:
                RETURN_IF_IERROR(bejHandleBejSet(&params));
                break;
            case bejArray:
                RETURN_IF_IERROR(bejHandleBejArray(&params));
                break;
            case bejNull:
                RETURN_IF_IERROR(bejHandleBejNull(&params));
                break;
            case bejInteger:
                RETURN_IF_IERROR(bejHandleBejInteger(&params));
                break;
            case bejEnum:
                RETURN_IF_IERROR(bejHandleBejEnum(&params));
                break;
            case bejString:
                RETURN_IF_IERROR(bejHandleBejString(&params));
                break;
            case bejReal:
                RETURN_IF_IERROR(bejHandleBejReal(&params));
                break;
            case bejBoolean:
                RETURN_IF_IERROR(bejHandleBejBoolean(&params));
                break;
            case bejBytestring:
                // TODO: Add support for BejBytestring decoding.
                fprintf(stderr, "No BejBytestring support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case bejChoice:
                // TODO: Add support for BejChoice decoding.
                fprintf(stderr, "No BejChoice support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case bejPropertyAnnotation:
                RETURN_IF_IERROR(bejHandleBejPropertyAnnotation(&params));
                break;
            case bejResourceLink:
                // TODO: Add support for BejResourceLink decoding.
                fprintf(stderr, "No BejResourceLink support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            case bejResourceLinkExpansion:
                // TODO: Add support for BejResourceLinkExpansion decoding.
                fprintf(stderr, "No BejResourceLinkExpansion support\n");
                params.state.encodedStreamOffset = params.sflv.valueEndOffset;
                break;
            default:
                break;
        }
    }
    RETURN_IF_IERROR(bejProcessEnding(&params, /*canBeEmpty=*/true));
    if (!params.stackCallback->stackEmpty(params.stackDataPtr))
    {
        fprintf(stderr, "Ending stack should be empty but its not. Something "
                        "must have gone wrong with the encoding\n");
        return bejErrorUnknown;
    }
    return 0;
}

/**
 * @brief Check if a bej version is supported by this decoder
 *
 * @param[in] bejVersion - the bej version in the received encoded stream
 * @return true if supported.
 */
static bool bejIsSupported(uint32_t bejVersion)
{
    for (uint32_t i = 0; i < sizeof(supportedBejVersions) / sizeof(uint32_t);
         ++i)
    {
        if (bejVersion == supportedBejVersions[i])
        {
            return true;
        }
    }
    return false;
}

int bejDecodePldmBlock(const struct BejDictionaries* dictionaries,
                       const uint8_t* encodedPldmBlock, uint32_t blockLength,
                       const struct BejStackCallback* stackCallback,
                       const struct BejDecodedCallback* decodedCallback,
                       void* callbacksDataPtr, void* stackDataPtr)
{
    NULL_CHECK(dictionaries, "dictionaries");
    NULL_CHECK(dictionaries->schemaDictionary, "schemaDictionary");
    NULL_CHECK(dictionaries->annotationDictionary, "annotationDictionary");

    NULL_CHECK(encodedPldmBlock, "encodedPldmBlock");

    NULL_CHECK(stackCallback, "stackCallback");
    NULL_CHECK(stackCallback->stackEmpty, "stackEmpty");
    NULL_CHECK(stackCallback->stackPeek, "stackPeek");
    NULL_CHECK(stackCallback->stackPop, "stackPop");
    NULL_CHECK(stackCallback->stackPush, "stackPush");

    NULL_CHECK(decodedCallback, "decodedCallback");

    uint32_t pldmHeaderSize = sizeof(struct BejPldmBlockHeader);
    if (blockLength < pldmHeaderSize)
    {
        fprintf(stderr, "Invalid pldm block size: %u\n", blockLength);
        return bejErrorInvalidSize;
    }

    const struct BejPldmBlockHeader* pldmHeader =
        (const struct BejPldmBlockHeader*)encodedPldmBlock;

    if (!bejIsSupported(pldmHeader->bejVersion))
    {
        fprintf(stderr, "Bej decoder doesn't support the bej version: %u\n",
                pldmHeader->bejVersion);
        return bejErrorNotSuppoted;
    }

    if (pldmHeader->schemaClass == bejAnnotationSchemaClass)
    {
        fprintf(stderr,
                "Encoder schema class cannot be BejAnnotationSchemaClass\n");
        return bejErrorNotSuppoted;
    }
    // TODO: Add support for CollectionMemberType schema class.
    if (pldmHeader->schemaClass == bejCollectionMemberTypeSchemaClass)
    {
        fprintf(stderr, "Decoder doesn't support "
                        "bejCollectionMemberTypeSchemaClass yet.\n");
        return bejErrorNotSuppoted;
    }
    // TODO: Add support for Error schema class.
    if (pldmHeader->schemaClass == bejErrorSchemaClass)
    {
        fprintf(stderr, "Decoder doesn't support BejErrorSchemaClass yet.\n");
        return bejErrorNotSuppoted;
    }

    // Skip the PLDM header.
    const uint8_t* enStream = encodedPldmBlock + pldmHeaderSize;
    uint32_t streamLen = blockLength - pldmHeaderSize;
    return bejDecode(dictionaries->schemaDictionary,
                     dictionaries->annotationDictionary, enStream, streamLen,
                     stackCallback, decodedCallback, callbacksDataPtr,
                     stackDataPtr);
}
