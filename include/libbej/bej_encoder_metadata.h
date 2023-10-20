#pragma once

#include "bej_common.h"
#include "bej_tree.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Update the node metadata used during encoding process.
 *
 * This function will visit the entire JSON tree and update metdata
 * properties of each node.
 *
 * @param dictionaries - dictionaries used for encoding.
 * @param majorSchemaStartingOffset - starting dictionary offset for
 * endcoding. Use BEJ_DICTIONARY_START_AT_HEAD to encode a complete
 * resource. Use the correct offset when encoding a subsection of a redfish
 * resource.
 * @param root - root node of the resource to be encoded. Root node has to
 * be a bejSet.
 * @param stack - An intialized BejEncoderOutputHandler struct.
 * @return 0 if successful.
 */
int bejUpdateNodeMetadata(const struct BejDictionaries* dictionaries,
                          uint16_t majorSchemaStartingOffset,
                          struct RedfishPropertyParent* root,
                          struct BejPointerStackCallback* stack);

#ifdef __cplusplus
}
#endif
