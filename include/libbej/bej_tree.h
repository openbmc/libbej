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
     * @brief Holds info needed to encode a parent in the JSON tree.
     */
    struct BejEncoderParentMetaData
    {
        // Starting dictionary index of the children properties.
        uint16_t childrenDictPropOffset;
        // Index of the child pointed by nextChild. Used to store the array
        // indexes of bejArray elements.
        uint16_t nextChildIndex;
        // BEJ sequence number of the property.
        uint32_t sequenceNumber;
        // Size needed to encode Sequence number, Format and Length of the
        // value.
        size_t sflSize;
        // Size of the value.
        size_t vSize;
        // Dictionary used for this parent.
        const uint8_t* dictionary;
        // Points to the next node which is need to process.
        void* nextChild;
    };

    /**
     * @brief Holds info needed to encode a leaf type in the JSON tree.
     */
    struct BejEncoderLeafMetaData
    {
        // BEJ sequence number of the property.
        uint32_t sequenceNumber;
        // Size needed to encode Sequence number, Format and Length of the
        // value.
        size_t sflSize;
        // Size of the value.
        size_t vSize;
    };

    /**
     * @brief Common attributes of a JSON property.
     */
    struct RedfishPropertyNode
    {
        const char* name;
        struct BejTupleF format;
        // Properties belonging to the same set or
        // array and has the same depth.
        void* sibling;
    };

    /**
     * @brief Used to store parent type property info.
     *
     * bejArray, bejSet and bejPropertyAnnotation are the parent type nodes.
     */
    struct RedfishPropertyParent
    {
        // Common property attributes.
        struct RedfishPropertyNode nodeAttr;
        // Number of children in the case of bejSet
        // or bejArray.
        size_t nChildren;
        //  Points to the first child.
        void* firstChild;
        // Points to the last child. We could keep only the firstChild pointer
        // and add a new element to the beginning of a list rather than to the
        // end. Then in order to maintain the array element order, when adding
        // elements to arrays, user has to add the last element first and so on.
        // But it's more easier to add elements in forward order. Furthermore
        // new elements can get added to the end of an array. Then we need to
        // reconstruct the list to represent that array.
        void* lastChild;
        // Metadata used during encoding.
        struct BejEncoderParentMetaData metaData;
    };

    /**
     * @brief Used to store leaf type property info.
     *
     * Every type that doesn't belong to parent type are considered as a leaf
     * property within a JSON tree. Each specific leaf type has its own struct.
     * They should include this struct as the first member of their struct.
     */
    struct RedfishPropertyLeaf
    {
        struct RedfishPropertyNode nodeAttr;
        struct BejEncoderLeafMetaData metaData;
    };

    /**
     * @brief bejInteger type property node.
     */
    struct RedfishPropertyLeafInt
    {
        struct RedfishPropertyLeaf leaf;
        int64_t value;
    };

    /**
     * @brief bejEnum type property node.
     */
    struct RedfishPropertyLeafEnum
    {
        struct RedfishPropertyLeaf leaf;
        // A string representation of the enum value.
        const char* value;
        // Sequence number of the enum value. Populated during bej encoding.
        uint16_t enumValueSeq;
    };

    /**
     * @brief bejString type property node.
     */
    struct RedfishPropertyLeafString
    {
        struct RedfishPropertyLeaf leaf;
        const char* value;
    };

    /**
     * @brief bejReal type property node.
     */
    struct RedfishPropertyLeafReal
    {
        struct RedfishPropertyLeaf leaf;
        double value;
        // bejReal representation of the value. Populated during bej encoding.
        struct BejReal bejReal;
    };

    /**
     * @brief bejBoolean type property node.
     */
    struct RedfishPropertyLeafBool
    {
        struct RedfishPropertyLeaf leaf;
        bool value;
    };

    /**
     * @brief Initialize a bejSet type node.
     *
     * @param[in] node - pointer to a RedfishPropertyParent struct.
     * @param[in] name - name of the node.
     */
    void bejTreeInitSet(struct RedfishPropertyParent* node, const char* name);

    /**
     * @brief Initialize a bejArray type node.
     *
     * @param[in] node - pointer to a RedfishPropertyParent struct.
     * @param[in] name - name of the node.
     */
    void bejTreeInitArray(struct RedfishPropertyParent* node, const char* name);

    /**
     * @brief Initialize a bejPropertyAnnotation type node.
     *
     * @param[in] node - pointer to a RedfishPropertyParent struct.
     * @param[in] name - name of the node.
     */
    void bejTreeInitPropertyAnnotated(struct RedfishPropertyParent* node,
                                      const char* name);

    /**
     * @brief Add a bejInteger type node to a parent node.
     *
     * @param[in] parent - a pointer to an initialized parent struct.
     * @param[in] child - a pointer to an uninitialized bejInteger type node.
     * @param[in] name - name of the bejInteger type property.
     * @param[in] value - value of the bejInteger type property.
     */
    void bejTreeAddInteger(struct RedfishPropertyParent* parent,
                           struct RedfishPropertyLeafInt* child,
                           const char* name, int64_t value);

    /**
     * @brief Set a new value in bejInteger type node.
     *
     * @param[in] node - initialized bejInteger type node.
     * @param[in] newValue - new integer value.
     */
    void bejTreeSetInteger(struct RedfishPropertyLeafInt* node,
                           int64_t newValue);

    /**
     * @brief Add a bejEnum type node to a parent node.
     *
     * @param[in] parent - a pointer to an initialized parent struct.
     * @param[in] child - a pointer to an uninitialized bejEnum type node.
     * @param[in] name - name of the bejEnum type property.
     * @param[in] value - value of the bejEnum type property.
     */
    void bejTreeAddEnum(struct RedfishPropertyParent* parent,
                        struct RedfishPropertyLeafEnum* child, const char* name,
                        const char* value);

    /**
     * @brief Link a node to its parent.
     *
     * @param[in] parent  - a pointer to an initialized parent struct.
     * @param[in] child - a pointer to an initialized child struct.
     */
    void bejTreeLinkChildToParent(struct RedfishPropertyParent* parent,
                                  void* child);

#ifdef __cplusplus
}
#endif
