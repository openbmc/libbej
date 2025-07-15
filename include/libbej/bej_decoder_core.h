#pragma once

#include "bej_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Indicates whether a new BEJ section falls inside a BEJ array or a
 * BEJ set or none of those.
 */
enum BejSectionType
{
    bejSectionNoType,
    bejSectionSet,
    bejSectionArray,
};

/**
 * @brief These stack entries are needed to implement the decoding
 * non-recursively.
 */
struct BejStackProperty
{
    // Indicates whether we are inside an array or a set or an annotation.
    enum BejSectionType sectionType;
    // Indicate whether we have property names for properties.
    bool addPropertyName;
    // Offset to the parent property in schema dictionary.
    uint16_t mainDictPropOffset;
    // Offset to the parent property in annotation dictionary.
    uint16_t annoDictPropOffset;
    // Offset to the end of the array or set or annotation.
    uint32_t streamEndOffset;
};

/**
 * @brief Holds the information related to the current bejTuple being
 * decoded.
 */
struct BejDecoderStates
{
    bool addPropertyName;
    uint16_t mainDictPropOffset;
    uint16_t annoDictPropOffset;
    uint32_t encodedStreamOffset;
    const uint8_t* encodedSubStream;
};

/**
 * @brief Callbacks for decoded data.
 *
 * dataPtr in the callback functions can be used for extra arguments.
 */
struct BejDecodedCallback
{
    /**
     * @brief Calls when a Set is detected.
     */
    int (*callbackSetStart)(const char* propertyName, void* dataPtr);

    /**
     * @brief Calls when an end of a Set is found.
     */
    int (*callbackSetEnd)(void* dataPtr);

    /**
     * @brief Calls when an array is detected.
     */
    int (*callbackArrayStart)(const char* propertyName, void* dataPtr);

    /**
     * @brief Calls when an end of an array is found.
     */
    int (*callbackArrayEnd)(void* dataPtr);

    /**
     * @brief Calls after a property is finished unless this is the last
     * property in a Set or an array. In that case appropriate
     * callbackSetEnd or callbackArrayEnd will be called.
     */
    int (*callbackPropertyEnd)(void* dataPtr);

    /**
     * @brief Calls when a Null property is found or the property length is
     * 0.
     */
    int (*callbackNull)(const char* propertyName, void* dataPtr);

    /**
     * @brief Calls when an Integer property is found.
     */
    int (*callbackInteger)(const char* propertyName, int64_t value,
                           void* dataPtr);

    /**
     * @brief Calls when an Enum property is found.
     */
    int (*callbackEnum)(const char* propertyName, const char* value,
                        void* dataPtr);

    /**
     * @brief Calls when a String property is found.
     */
    int (*callbackString)(const char* propertyName, const char* value,
                          size_t length, void* dataPtr);

    /**
     * @brief Calls when a Real value property is found.
     */
    int (*callbackReal)(const char* propertyName, const struct BejReal* value,
                        void* dataPtr);

    /**
     * @brief Calls when a Bool property is found.
     */
    int (*callbackBool)(const char* propertyName, bool value, void* dataPtr);

    /**
     * @brief Calls when an Annotated property is found.
     */
    int (*callbackAnnotation)(const char* propertyName, void* dataPtr);

    /**
     * @brief Calls when a read only property is found.
     */
    int (*callbackReadonlyProperty)(uint32_t sequenceNumber, void* dataPtr);
};

/**
 * @brief Stack for holding BejStackProperty types. Decoder core is not
 * responsible for creating or deleting stack memory. User of the decoder
 * core is responsible for creating and deleting stack memory.
 *
 * dataPtr in the callback functions can be used for extra arguments.
 */
struct BejStackCallback
{
    /**
     * @brief Return true if the stack is empty.
     */
    bool (*stackEmpty)(void* dataPtr);

    /**
     * @brief View the object at the top of the stack. If the stack is
     * empty, this will return NULL.
     */
    const struct BejStackProperty* (*stackPeek)(void* dataPtr);

    /**
     * @brief Removes the top most object from the stack. Client of the
     * decoder core is responsible for destroying the memory for the removed
     * object.
     */
    void (*stackPop)(void* dataPtr);

    /**
     * @brief Push an object into the stack. Returns 0 if the operation is
     * successfull. Client of the decoder core is responsible for allocating
     * memory for the new object.
     */
    int (*stackPush)(const struct BejStackProperty* const property,
                     void* dataPtr);
};

/**
 * @brief Used to pass parameters to BEJ decoding local functions.
 */
struct BejHandleTypeFuncParam
{
    struct BejDecoderStates state;
    struct BejSFLV sflv;
    const uint8_t* mainDictionary;
    const uint8_t* annotDictionary;
    const struct BejDecodedCallback* decodedCallback;
    const struct BejStackCallback* stackCallback;
    void* callbacksDataPtr;
    void* stackDataPtr;
};

/**
 * @brief Decodes a PLDM block. Maximum encoded stream size the decoder
 * supports is 32bits.
 *
 * @param[in] dictionaries - dictionaries needed for decoding.
 * @param[in] encodedPldmBlock - encoded PLDM block.
 * @param[in] blockLength - length of the PLDM block.
 * @param[in] stackCallback - callbacks for stack handlers. callbacks in
 * stackCallback struct should be set to valid functions.
 * @param[in] decodedCallback - callbacks for extracting decoded
 * properties. callbacks in decodedCallback struct should be set to
 * NULL or valid functions.
 * @param[in] callbacksDataPtr - data pointer to pass to decoded callbacks.
 * This can be used pass additional data.
 * @param[in] stackDataPtr - data pointer to pass to stack callbacks. This
 * can be used pass additional data.
 *
 * @return 0 if successful.
 */
int bejDecodePldmBlock(const struct BejDictionaries* dictionaries,
                       const uint8_t* encodedPldmBlock, uint32_t blockLength,
                       const struct BejStackCallback* stackCallback,
                       const struct BejDecodedCallback* decodedCallback,
                       void* callbacksDataPtr, void* stackDataPtr);

#ifdef __cplusplus
}
#endif
