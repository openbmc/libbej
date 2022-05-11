#include "rde_common.h"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace rde
{

TEST(RdeCommonTest, RdeGetUnsignedIntegerTest)
{
    constexpr uint8_t bytes[] = {0xab, 0xcd, 0xef, 0x12,
                                 0x13, 0x65, 0x23, 0x89};
    EXPECT_THAT(rdeGetUnsignedInteger(bytes, /*numOfBytes=*/1), 0xab);
    EXPECT_THAT(rdeGetUnsignedInteger(bytes, /*numOfBytes=*/2), 0xcdab);
    EXPECT_THAT(rdeGetUnsignedInteger(bytes, /*numOfBytes=*/5), 0x1312efcdab);
    EXPECT_THAT(rdeGetUnsignedInteger(bytes, /*numOfBytes=*/8),
                0x8923651312efcdab);
}

TEST(RdeCommonTest, RdeGetNnintTest)
{
    constexpr uint8_t nnint1[] = {0x03, 0xcd, 0xef, 0x12};
    constexpr uint8_t nnint2[] = {0x08, 0xab, 0xcd, 0xef, 0x12,
                                  0x13, 0x65, 0x23, 0x89};
    EXPECT_THAT(rdeGetNnint(nnint1), 0x12efcd);
    EXPECT_THAT(rdeGetNnint(nnint2), 0x8923651312efcdab);
}

TEST(RdeCommonTest, RdeGetNnintSizeTest)
{
    constexpr uint8_t nnint1[] = {0x03, 0xcd, 0xef, 0x12};
    constexpr uint8_t nnint2[] = {0x08, 0xab, 0xcd, 0xef, 0x12,
                                  0x13, 0x65, 0x23, 0x89};
    EXPECT_THAT(rdeGetNnintSize(nnint1), 4);
    EXPECT_THAT(rdeGetNnintSize(nnint2), 9);
}

} // namespace rde