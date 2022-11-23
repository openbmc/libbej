#include "bej_common.h"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libbej
{

TEST(BejCommonTest, BejGetUnsignedIntegerTest)
{
    constexpr uint8_t bytes[] = {0xab, 0xcd, 0xef, 0x12,
                                 0x13, 0x65, 0x23, 0x89};
    EXPECT_THAT(bejGetUnsignedInteger(bytes, /*numOfBytes=*/1), 0xab);
    EXPECT_THAT(bejGetUnsignedInteger(bytes, /*numOfBytes=*/2), 0xcdab);
    EXPECT_THAT(bejGetUnsignedInteger(bytes, /*numOfBytes=*/5), 0x1312efcdab);
    EXPECT_THAT(bejGetUnsignedInteger(bytes, /*numOfBytes=*/8),
                0x8923651312efcdab);
}

TEST(BejCommonTest, BejGetNnintTest)
{
    constexpr uint8_t nnint1[] = {0x03, 0xcd, 0xef, 0x12};
    constexpr uint8_t nnint2[] = {0x08, 0xab, 0xcd, 0xef, 0x12,
                                  0x13, 0x65, 0x23, 0x89};
    EXPECT_THAT(bejGetNnint(nnint1), 0x12efcd);
    EXPECT_THAT(bejGetNnint(nnint2), 0x8923651312efcdab);
}

TEST(BejCommonTest, BejGetNnintSizeTest)
{
    constexpr uint8_t nnint1[] = {0x03, 0xcd, 0xef, 0x12};
    constexpr uint8_t nnint2[] = {0x08, 0xab, 0xcd, 0xef, 0x12,
                                  0x13, 0x65, 0x23, 0x89};
    EXPECT_THAT(bejGetNnintSize(nnint1), 4);
    EXPECT_THAT(bejGetNnintSize(nnint2), 9);
}

} // namespace libbej
