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
        bejErrorNoError = 0,
        bejErrorUnknown,
        bejErrorInvalidSize,
        bejErrorNotSuppoted,
        bejErrorUnknownProperty,
        bejErrorInvalidSchemaType,
        bejErrorInvalidPropertyOffset,
        bejErrorNullParameter,
    };

    /**
     * @brief BEJ schema classes.
     */
    enum BejSchemaClass
    {
        bejMajorSchemaClass = 0,
        bejEventSchemaClass = 1,
        bejAnnotationSchemaClass = 2,
        bejCollectionMemberTypeSchemaClass = 3,
        bejErrorSchemaClass = 4,
    };

    /**
     * @brief BEJ data types supported in BEJ version 0xF1F0F000.
     */
    enum BejPrincipalDataType
    {
        bejSet = 0,
        bejArray = 1,
        bejNull = 2,
        bejInteger = 3,
        bejEnum = 4,
        bejString = 5,
        bejReal = 6,
        bejBoolean = 7,
        bejBytestring = 8,
        bejChoice = 9,
        bejPropertyAnnotation = 10,
        bejPrincipalDataReserved1 = 11,
        bejPrincipalDataReserved2 = 12,
        bejPrincipalDataReserved3 = 13,
        bejResourceLink = 14,
        bejResourceLinkExpansion = 15,
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

    /**
     * @brief Get the unsigned integer value from provided bytes.
     *
     * @param[in] bytes - valid pointer to a byte stream in little-endian
     * format.
     * @param[in] numOfBytes - number of bytes belongs to the value. Maximum
     * number of bytes supported is 8. If numOfBytes > 8, the result is
     * undefined.
     * @return unsigend 64bit representation of the value.
     */
    uint64_t rdeGetUnsignedInteger(const uint8_t* bytes, uint8_t numOfBytes);

    /**
     * @brief Get the value from nnint type.
     *
     * @param[in] nnint - nnint should be pointing to a valid nnint.
     * @return unsigend 64bit representation of the value.
     */
    uint64_t rdeGetNnint(const uint8_t* nnint);

    /**
     * @brief Get the size of the complete nnint.
     *
     * @param[in] nnint - pointer to a valid nnint.
     * @return size of the complete nnint.
     */
    uint8_t rdeGetNnintSize(const uint8_t* nnint);

#ifdef __cplusplus
}
#endif
