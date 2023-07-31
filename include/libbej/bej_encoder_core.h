#pragma once

#include "bej_tree.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct BejEncoderOutputHandler
    {
        void* handlerContext;
        int (*recvOutput)(void* data, size_t data_size, void* handlerContext);
    };

    int bejEncode(const struct BejDictionaries* dictionaries,
                  uint16_t majorSchemaStartingOffset,
                  enum BejSchemaClass schemaClass,
                  struct RedfishPropertyParent* root,
                  struct BejEncoderOutputHandler* output,
                  struct BejPointerStackCallback* stack);

#ifdef __cplusplus
}
#endif
