#pragma once

#include "bej_tree.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief A struct for storing output information for the encoder.
     */
    struct BejEncoderOutputHandler
    {
        // A user provided context to be passed to the recvOutput() callback.
        void* handlerContext;
        // A callback function that will be invoked by the encoder whenever it
        // has output data.
        int (*recvOutput)(const void* data, size_t data_size,
                          void* handlerContext);
    };

    /**
     * @brief Perform BEJ encoding.
     *
     * @param dictionaries - dictionaries used for encoding.
     * @param majorSchemaStartingOffset - starting dictionary offset for
     * endcoding. Use BEJ_DICTIONARY_START_AT_HEAD to encode a complete
     * resource. Use the correct offset when encoding a subsection of a redfish
     * resource.
     * @param schemaClass - schema class for the resource.
     * @param root - root node of the resource to be encoded. Root node has to
     * be a bejSet.
     * @param output - An intialized BejEncoderOutputHandler struct.
     * @param stack - An initialized BejPointerStackCallback struct.
     * @return 0 if successful.
     */
    int bejEncode(const struct BejDictionaries* dictionaries,
                  uint16_t majorSchemaStartingOffset,
                  enum BejSchemaClass schemaClass,
                  struct RedfishPropertyParent* root,
                  struct BejEncoderOutputHandler* output,
                  struct BejPointerStackCallback* stack);

#ifdef __cplusplus
}
#endif
