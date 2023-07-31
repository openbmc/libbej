#pragma once

#include "bej_common.h"
#include "bej_tree.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Updates the each node's sequence number and value size metadata fields.

    /**
     * @brief
     *
     * @param dictionaries
     * @param majorSchemaStartingOffset
     * @param root
     * @param stack
     * @return int
     */
    int bejUpdateNodeMetadata(const struct BejDictionaries* dictionaries,
                              uint16_t majorSchemaStartingOffset,
                              struct RedfishPropertyParent* root,
                              struct BejPointerStackCallback* stack);

#ifdef __cplusplus
}
#endif
