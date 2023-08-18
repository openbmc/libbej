#pragma once

#include "bej_common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Mask for the type of the dictionary within a bejTupleS.
 */
#define DICTIONARY_TYPE_MASK 0x01

/**
 * @brief Number of bits needed to shift to get the sequence number from a
 * bejTupleS nnint value.
 */
#define DICTIONARY_SEQ_NUM_SHIFT 1

    /**
     * @brief BEJ dictionary type.
     */
    enum BejDictionaryType
    {
        bejPrimary = 0,
        bejAnnotation = 1,
    };

    /**
     * @brief Dictionary property header.
     */
    struct BejDictionaryProperty
    {
        struct BejTupleF format;
        uint16_t sequenceNumber;
        uint16_t childPointerOffset;
        uint16_t childCount;
        uint8_t nameLength;
        uint16_t nameOffset;
    } __attribute__((__packed__));

    struct BejDictionaryHeader
    {
        uint8_t versionTag;
        uint8_t truncationFlag : 1;
        uint8_t reservedFlags : 7;
        uint16_t entryCount;
        uint32_t schemaVersion;
        uint32_t dictionarySize;
    } __attribute__((__packed__));

    /**
     * @brief Get the offset of the first property in a dictionary.
     *
     * @return the offset to the first property.
     */
    uint16_t bejDictGetPropertyHeadOffset();

    /**
     * @brief Get the offset of the first annotated property in an annoation
     * dictionary.
     *
     * @return the offset to the first annotated property in an annoation
     * dictionary.
     */
    uint16_t bejDictGetFirstAnnotatedPropertyOffset();

    /**
     * @brief Get the property related to the given sequence number.
     *
     * @param[in] dictionary - dictionary containing the sequence number.
     * @param[in] startingPropertyOffset - offset of the starting property for
     * the search.
     * @param[in] sequenceNumber - sequence number of the property.
     * @param[out] property - if the search is successful, this will point to a
     * valid property.
     * @param[out] propertyOffset - if the search is successful, this will point
     * to the offset of the property within the dictionary. Can provide a NULL
     * pointer if this is not needed.
     * @return 0 if successful.
     */
    int bejDictGetProperty(const uint8_t* dictionary,
                           uint16_t startingPropertyOffset,
                           uint16_t sequenceNumber,
                           const struct BejDictionaryProperty** property,
                           uint16_t* propertyOffset);

    /**
     * @brief Get the name of a property.
     *
     * @param[in] dictionary - dictionary containing the property.
     * @param[in] nameOffset - dictionary offset of the name.
     * @param[in] nameLength - length of the name.
     * @return a NULL terminated string. If the nameLength is 0, this will
     * return an empty string.
     */
    const char* bejDictGetPropertyName(const uint8_t* dictionary,
                                       uint16_t nameOffset, uint8_t nameLength);

    /**
     * @brief Get the property related to the given property name.
     *
     * @param[in] dictionary - dictionary containing the property.
     * @param[in] startingPropertyOffset - offset of the starting property for
     * the search.
     * @param[in] propertyName - name of the searched property.
     * @param[out] property - if the search is successful, this will point to a
     * valid property.
     * @param[out] propertyOffset - if the search is successful, this will point
     * to the offset of the property within the dictionary. Can provide a NULL
     * pointer if this is not needed.
     * @return 0 if successful.
     */
    int bejDictGetPropertyByName(const uint8_t* dictionary,
                                 uint16_t startingPropertyOffset,
                                 const char* propertyName,
                                 const struct BejDictionaryProperty** property,
                                 uint16_t* propertyOffset);

    /**
     * @brief Get dictionary entry pointed by the bejLocator.
     *
     * @param dictionary - dictionary containing the property.
     * @param bejLocator - bejLocator data.
     * @param locatorLength - length of the locatorLength.
     * @param property - if successful, this will point to a valid property if
     * property is not NULL.
     * @param property_offset - if successful, this provides the dictionary
     * offset of the found property.
     * @return 0 if successful.
     */
    int bejDictEntryByBejLocator(const uint8_t* dictionary,
                                 const uint8_t* bejLocator,
                                 uint8_t locatorLength,
                                 const struct BejDictionaryProperty** property,
                                 uint16_t* propertyOffset);

#ifdef __cplusplus
}
#endif
