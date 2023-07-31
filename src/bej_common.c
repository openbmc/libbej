#include "bej_common.h"

uint64_t bejGetUnsignedInteger(const uint8_t* bytes, uint8_t numOfBytes)
{
    uint64_t num = 0;
    for (uint8_t i = 0; i < numOfBytes; ++i)
    {
        num |= (uint64_t)(*(bytes + i)) << (i * 8);
    }
    return num;
}

uint64_t bejGetNnint(const uint8_t* nnint)
{
    // In nnint, first byte indicate how many bytes are there. Remaining bytes
    // represent the value in little-endian format.
    const uint8_t size = *nnint;
    return bejGetUnsignedInteger(nnint + sizeof(uint8_t), size);
}

uint8_t bejGetNnintSize(const uint8_t* nnint)
{
    // In nnint, first byte indicate how many bytes are there.
    return *nnint + sizeof(uint8_t);
}

uint8_t bejNnintEncodedingSizeOfUInt(uint64_t val)
{
    uint8_t bytes = 0;
    do
    {
        // Even if the value is 0, we need a byte for that.
        ++bytes;
        val = val >> 8;
    } while (val != 0);
    // Need 1 byte to add the nnint length.
    return bytes + 1;
}

uint8_t bejNnintLengthFieldOfUInt(uint64_t val)
{
    // From the size of the encoded value, we need 1 byte for the length field.
    return bejNnintEncodedingSizeOfUInt(val) - 1;
}
