#include "bej_dictionary.h"

#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Get the index for a property offset. First property will be at index
 * 0.
 *
 * @param propertyOffset - A valid property offset.
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
 * @param propertyOffset - Offset needed to be validated.
 * @return true if propertyOffset is a valid offset.
 */
static bool bejValidatePropertyOffset(uint16_t propertyOffset)
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
                       const struct BejDictionaryProperty** property)
{
    uint16_t propertyOffset = startingPropertyOffset;
    const struct BejDictionaryHeader* header =
        (const struct BejDictionaryHeader*)dictionary;

    if (!bejValidatePropertyOffset(propertyOffset))
    {
        return BejErrorInvalidPropertyOffset;
    }
    uint16_t propertyIndex = bejGetPropertyEntryIndex(propertyOffset);

    for (uint16_t index = propertyIndex; index < header->entryCount; ++index)
    {
        const struct BejDictionaryProperty* p =
            (const struct BejDictionaryProperty*)(dictionary + propertyOffset);
        if (p->sequenceNumber == sequenceNumber)
        {
            *property = p;
            return 0;
        }
        propertyOffset += sizeof(struct BejDictionaryProperty);
    }
    return BejErrorUnknownProperty;
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
