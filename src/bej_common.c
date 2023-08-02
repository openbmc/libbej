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

uint8_t bejIntLengthOfValue(int64_t val)
{
    // Only need to encode 0x00 or 0xFF
    if (val == 0 || val == -1)
    {
        return 1;
    }

    // Starts at the MSB. LSB index is 0.
    uint8_t byteIndex = sizeof(uint64_t) - 1;
    const uint8_t bitsPerByte = 8;
    // The current byte being looked at. Starts at MSB.
    uint8_t currentByte = (val >> (bitsPerByte * byteIndex)) & 0xFF;
    uint8_t byteLength = sizeof(int64_t);

    while ((val > 0 && currentByte == 0) || (val < 0 && currentByte == 0xFF))
    {
        byteLength--;
        byteIndex--;
        currentByte = (val >> (bitsPerByte * byteIndex)) & 0xFF;
    }

    // If the value is positive and encoded MSBbit is 1 we need to add 0x00 to
    // the encoded value as padding.
    if (val > 0 && (currentByte & 0x80))
    {
        byteLength++;
    }

    // If the value is negative and encoded MSBbit is 0 we need to add 0xFF to
    // the encoded value as padding.
    if (val < 0 && !(currentByte & 0x80))
    {
        byteLength++;
    }

    return byteLength;
}

uint8_t bejNnintEncodingSizeOfUInt(uint64_t val)
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
    return bejNnintEncodingSizeOfUInt(val) - 1;
}
