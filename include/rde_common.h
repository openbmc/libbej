#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief If expr is non zero, return with that value.
 */
#ifndef RETURN_IF_IERROR
#define RETURN_IF_IERROR(expr)                                                 \
    do                                                                         \
    {                                                                          \
        int __status = (expr);                                                 \
        if (__status != 0)                                                     \
        {                                                                      \
            return __status;                                                   \
        }                                                                      \
    } while (0)
#endif

    /**
     * @brief RDE BEJ decoding errors.
     */
    enum BejError
    {
        BejErrorNoError = 0,
        BejErrorUnknown,
        BejErrorInvalidSize,
        BejErrorNotSuppoted,
        BejErrorUnknownProperty,
        BejErrorInvalidSchemaType,
        BejErrorInvalidPropertyOffset,
    };

    /**
     * @brief BEJ schema classes.
     */
    enum BejSchemaClass
    {
        BejMajorSchemaClass = 0,
        BejEventSchemaClass = 1,
        BejAnnotationSchemaClass = 2,
        BejCollectionMemberTypeSchemaClass = 3,
        BejErrorSchemaClass = 4,
    };

    /**
     * @brief BEJ data types supported in BEJ version 0xF1F0F000.
     */
    enum BejPrincipalDataType
    {
        BejSet = 0,
        BejArray = 1,
        BejNull = 2,
        BejInteger = 3,
        BejEnum = 4,
        BejString = 5,
        BejReal = 6,
        BejBoolean = 7,
        BejBytestring = 8,
        BejChoice = 9,
        BejPropertyAnnotation = 10,
        Reserved1 = 11,
        Reserved2 = 12,
        Reserved3 = 13,
        BejResourceLink = 14,
        BejResourceLinkExpansion = 15,
    };

    /**
     * @brief Format BEJ tuple.
     */
    struct BejTupleF
    {
        uint8_t deferredBinding : 1;
        uint8_t readOnlyProperty : 1;
        uint8_t nullableProperty : 1;
        uint8_t reserved : 1;
        enum BejPrincipalDataType principalDataType : 4;
    } __attribute__((__packed__));

    /**
     * @brief Sequence Number BEJ tuple.
     */
    struct BejTupleS
    {
        uint8_t schema;
        // Dictionaries contain 16bit sequence numbers. So allocating 16bits for
        // the sequence number here.
        uint16_t sequenceNumber;
    };

    /**
     * @brief Represent offsets of Format, Value Length and Value of a SFLV
     * tuple.
     */
    struct BejSFLVOffset
    {
        uint32_t formatOffset;
        uint32_t valueLenNnintOffset;
        uint32_t valueOffset;
    };

    /**
     * @brief Fields in Bej Real data type.
     */
    struct BejReal
    {
        // Number bytes in exp.
        uint8_t expLen;
        int64_t whole;
        uint64_t zeroCount;
        uint64_t fract;
        int64_t exp;
    };

    /**
     * @brief SFLV BEJ tuple infomation.
     */
    struct BejSFLV
    {
        struct BejTupleS tupleS;
        struct BejTupleF format;
        // Value portion size in bytes.
        uint32_t valueLength;
        // Value end-offset with respect to the begining of the encoded stream.
        uint32_t valueEndOffset;
        // Pointer to the value.
        const uint8_t* value;
    };

    /**
     * @brief bejEncoding PLDM data type header.
     */
    struct BejPldmBlockHeader
    {
        uint32_t bejVersion;
        uint16_t reserved;
        uint8_t schemaClass;
    } __attribute__((__packed__));

    enum RdeOperationInitType
    {
        RdeOpInitOperationHead = 0,
        RdeOpInitOperationRead = 1,
        RdeOpInitOperationCreate = 2,
        RdeOpInitOperationDelete = 3,
        RdeOpInitOperationUpdate = 4,
        RdeOpInitOperationReplace = 5,
        RdeOpInitOperationAction = 6,
    };

    enum RdeMultiReceiveTransferFlag
    {
        RdeMRecFlagStart = 0,
        RdeMRecFlagMiddle = 1,
        RdeMRecFlagEnd = 2,
        RdeMRecFlagStartAndEnd = 3,
    };

    struct RdeOperationInitReqHeader
    {
        uint32_t resourceID;
        uint16_t operationID;
        uint8_t operationType;

        // OperationFlags bits
        uint8_t locatorValid : 1;
        uint8_t containsRequestPayload : 1;
        uint8_t containsCustomRequestParameters : 1;

        uint8_t reserved : 5;
        uint32_t sendDataTransferHandle;
        uint8_t operationLocatorLength;
        uint32_t requestPayloadLength;
    } __attribute__((__packed__));

    struct MultipartReceiveResHeader
    {
        uint8_t completionCode;
        uint8_t transferFlag;
        uint32_t nextDataTransferHandle;
        uint32_t dataLengthBytes;
    } __attribute__((__packed__));

    /**
     * @brief Get the unsigned integer value from provided bytes.
     *
     * @param bytes - valid pointer to a byte stream in little-endian format.
     * @param numOfBytes - number of bytes belongs to the value. Maximum number
     * of bytes supported is 8. If numOfBytes > 8, the result is undefined.
     * @return unsigend 64bit representation of the value.
     */
    uint64_t rdeGetUnsignedInteger(const uint8_t* bytes, uint8_t numOfBytes);

    /**
     * @brief Get the value from nnint type.
     *
     * @param  nnint - nnint should be pointing to a valid nnint.
     * @return unsigend 64bit representation of the value.
     */
    uint64_t rdeGetNnint(const uint8_t* nnint);

    /**
     * @brief Get the size of the complete nnint.
     *
     * @param nnint - pointer to a valid nnint.
     * @return size of the complete nnint.
     */
    uint8_t rdeGetNnintSize(const uint8_t* nnint);

#ifdef __cplusplus
}
#endif