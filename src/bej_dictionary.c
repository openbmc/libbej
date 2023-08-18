#include "bej_dictionary.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Get the index for a property offset. First property will be at index
 * 0.
 *
 * @param[in] propertyOffset - a valid property offset.
 * @return index of the property.
 */
static uint16_t bejGetPropertyEntryIndex(uint16_t propertyOffset)
{
    return (propertyOffset - bejDictGetPropertyHeadOffset()) /
           sizeof(struct BejDictionaryProperty);
}

/**
 * @brief  Validate a property offset.
 *
 * @param[in] dictionary - pointer to the dictionary.
 * @param[in] propertyOffset - offset needed to be validated.
 * @return true if propertyOffset is a valid offset.
 */
static bool bejValidatePropertyOffset(const uint8_t* dictionary,
                                      uint16_t propertyOffset)
{
    // propertyOffset should be greater than or equal to first property offset.
    if (propertyOffset < bejDictGetPropertyHeadOffset())
    {
        fprintf(
            stderr,
            "Invalid property offset. Pointing to Dictionary header data\n");
        return false;
    }

    // propertyOffset should be a multiple of sizeof(BejDictionaryProperty)
    // starting from first property within the dictionary.
    if ((propertyOffset - bejDictGetPropertyHeadOffset()) %
        sizeof(struct BejDictionaryProperty))
    {
        fprintf(stderr, "Invalid property offset. Does not point to beginning "
                        "of property\n");
        return false;
    }

    const struct BejDictionaryHeader* header =
        (const struct BejDictionaryHeader*)dictionary;
    uint16_t propertyIndex = bejGetPropertyEntryIndex(propertyOffset);
    if (propertyIndex >= header->entryCount)
    {
        fprintf(stderr,
                "Invalid property offset %u. It falls outside of dictionary "
                "properties\n",
                propertyOffset);
        return false;
    }

    return true;
}

uint16_t bejDictGetPropertyHeadOffset()
{
    // First property is present soon after the dictionary header.
    return sizeof(struct BejDictionaryHeader);
}

uint16_t bejDictGetFirstAnnotatedPropertyOffset()
{
    // The first property available is the "Annotations" set which is the parent
    // for all properties. Next immediate property is the first property we
    // need.
    return sizeof(struct BejDictionaryHeader) +
           sizeof(struct BejDictionaryProperty);
}

int bejDictGetProperty(const uint8_t* dictionary,
                       uint16_t startingPropertyOffset, uint16_t sequenceNumber,
                       const struct BejDictionaryProperty** property,
                       uint16_t* propertyOffset)
{
    NULL_CHECK(property, "property in bejDictGetProperty");

    uint16_t curPropertyOffset = startingPropertyOffset;
    const struct BejDictionaryHeader* header =
        (const struct BejDictionaryHeader*)dictionary;

    if (!bejValidatePropertyOffset(dictionary, curPropertyOffset))
    {
        return bejErrorInvalidPropertyOffset;
    }
    uint16_t propertyIndex = bejGetPropertyEntryIndex(curPropertyOffset);

    for (uint16_t index = propertyIndex; index < header->entryCount; ++index)
    {
        const struct BejDictionaryProperty* p =
            (const struct BejDictionaryProperty*)(dictionary +
                                                  curPropertyOffset);
        if (p->sequenceNumber == sequenceNumber)
        {
            *property = p;
            // propertyOffset is an optional output.
            if (propertyOffset != NULL)
            {
                *propertyOffset = curPropertyOffset;
            }
            return 0;
        }
        curPropertyOffset += sizeof(struct BejDictionaryProperty);
    }
    return bejErrorUnknownProperty;
}

const char* bejDictGetPropertyName(const uint8_t* dictionary,
                                   uint16_t nameOffset, uint8_t nameLength)
{
    if (nameLength == 0)
    {
        return "";
    }
    return (const char*)(dictionary + nameOffset);
}

int bejDictGetPropertyByName(const uint8_t* dictionary,
                             uint16_t startingPropertyOffset,
                             const char* propertyName,
                             const struct BejDictionaryProperty** property,
                             uint16_t* propertyOffset)
{
    NULL_CHECK(property, "property in bejDictGetPropertyByName");

    uint16_t curPropertyOffset = startingPropertyOffset;
    const struct BejDictionaryHeader* header =
        (const struct BejDictionaryHeader*)dictionary;

    if (!bejValidatePropertyOffset(dictionary, curPropertyOffset))
    {
        return bejErrorInvalidPropertyOffset;
    }
    uint16_t propertyIndex = bejGetPropertyEntryIndex(curPropertyOffset);

    for (uint16_t index = propertyIndex; index < header->entryCount; ++index)
    {
        const struct BejDictionaryProperty* p =
            (const struct BejDictionaryProperty*)(dictionary +
                                                  curPropertyOffset);
        if (strcmp(propertyName,
                   bejDictGetPropertyName(dictionary, p->nameOffset,
                                          p->nameLength)) == 0)
        {
            *property = p;
            // propertyOffset is an optional output.
            if (propertyOffset != NULL)
            {
                *propertyOffset = curPropertyOffset;
            }
            return 0;
        }
        curPropertyOffset += sizeof(struct BejDictionaryProperty);
    }
    return bejErrorUnknownProperty;
}

int bejDictEntryByBejLocator(const uint8_t* dictionary,
                             const uint8_t* bejLocator, uint8_t locatorLength,
                             const struct BejDictionaryProperty** property,
                             uint16_t* propertyOffset)
{
    NULL_CHECK(propertyOffset, "propertyOffset in bejDictEntryByBejLocator");

    // bejLocator offset 0 provides a nnint which provides the total size of the
    // sequence numbers.
    const uint8_t* lengthBytesNnint = bejLocator;
    // Get the total length of the sequence number series. This is done for a
    // data validity check. We do not really need this seqNumbersLen for the
    // rest of the calculations.
    uint8_t seqNumbersLen = (uint8_t)bejGetNnint(lengthBytesNnint);
    if (seqNumbersLen + bejGetNnintSize(lengthBytesNnint) != locatorLength)
    {
        fprintf(stderr,
                "Invalid LengthBytes field in the bejLocator. LengthBytes: %u "
                "locatorLength: %u\n",
                seqNumbersLen, locatorLength);
        return -1;
    }

    const struct BejDictionaryProperty* curProperty;
    uint16_t curPropertyOffset;

    // The first bejTupleS starts after the first nnint that provides the length
    // bytes.
    uint8_t tupleSOffset = bejGetNnintSize(bejLocator);
    // Get the offset to the first property of the dictionary.
    uint16_t dictOffset = bejDictGetPropertyHeadOffset();

    // Go through the series of sequence numbers and reach the property.
    while (tupleSOffset < locatorLength)
    {
        const uint8_t* tupleS = bejLocator + tupleSOffset;
        // Dictionary sequence numnber is 16bits. But the bejTupleS type also
        // adds the dictionary selection bit to the LSB. So we need to use
        // uint32_t.
        uint32_t seqWithDictFlag = (uint32_t)bejGetNnint(tupleS);
        // Extract the sequence number.
        uint16_t seq = (seqWithDictFlag >> 1) & 0xFFFF;

        int ret = bejDictGetProperty(dictionary, dictOffset, seq, &curProperty,
                                     &curPropertyOffset);
        if (ret != 0)
        {
            fprintf(stderr,
                    "Failed to find a property for the sequence number %u. "
                    "Started at dictionary offset: 0x%x index: %u\n",
                    seq, dictOffset, bejGetPropertyEntryIndex(dictOffset));
            return ret;
        }

        // Goto next tuple offset/.
        tupleSOffset += bejGetNnintSize(tupleS);

        // Check wether this is the last sequence number in the list.
        // (tupleSOffset > locatorLength), shouldn't happen. If it happens,
        // probably the input is invalid.
        if (tupleSOffset == locatorLength)
        {
            *propertyOffset = curPropertyOffset;
            // property is an optional output.
            if (property != NULL)
            {
                *property = curProperty;
            }
            return 0;
        }

        // Next sequence number is the sequence number of the child property of
        // the current property. So jump the child of the current dictionary
        // entry.
        dictOffset = curProperty->childPointerOffset;
    }

    fprintf(stderr, "Couldn't reach the expected property. Input is invalid or "
                    "somethig went wrong during calculation");
    return -1;
}
