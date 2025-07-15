#include "bej_dictionary.h"

#include <array>
#include <string_view>
#include <tuple>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libbej
{

/**
 * @brief A valid dictionary.
 */
constexpr std::array<uint8_t, 279> dummySimpleDict{
    {0x0,  0x0,  0xc,  0x0,  0x0,  0xf0, 0xf0, 0xf1, 0x17, 0x1,  0x0,  0x0,
     0x0,  0x0,  0x0,  0x16, 0x0,  0x5,  0x0,  0xc,  0x84, 0x0,  0x14, 0x0,
     0x0,  0x48, 0x0,  0x1,  0x0,  0x13, 0x90, 0x0,  0x56, 0x1,  0x0,  0x0,
     0x0,  0x0,  0x0,  0x3,  0xa3, 0x0,  0x74, 0x2,  0x0,  0x0,  0x0,  0x0,
     0x0,  0x16, 0xa6, 0x0,  0x34, 0x3,  0x0,  0x0,  0x0,  0x0,  0x0,  0x16,
     0xbc, 0x0,  0x64, 0x4,  0x0,  0x0,  0x0,  0x0,  0x0,  0x13, 0xd2, 0x0,
     0x0,  0x0,  0x0,  0x52, 0x0,  0x2,  0x0,  0x0,  0x0,  0x0,  0x74, 0x0,
     0x0,  0x0,  0x0,  0x0,  0x0,  0xf,  0xe5, 0x0,  0x46, 0x1,  0x0,  0x66,
     0x0,  0x3,  0x0,  0xb,  0xf4, 0x0,  0x50, 0x0,  0x0,  0x0,  0x0,  0x0,
     0x0,  0x9,  0xff, 0x0,  0x50, 0x1,  0x0,  0x0,  0x0,  0x0,  0x0,  0x7,
     0x8,  0x1,  0x50, 0x2,  0x0,  0x0,  0x0,  0x0,  0x0,  0x7,  0xf,  0x1,
     0x44, 0x75, 0x6d, 0x6d, 0x79, 0x53, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x0,
     0x43, 0x68, 0x69, 0x6c, 0x64, 0x41, 0x72, 0x72, 0x61, 0x79, 0x50, 0x72,
     0x6f, 0x70, 0x65, 0x72, 0x74, 0x79, 0x0,  0x49, 0x64, 0x0,  0x53, 0x61,
     0x6d, 0x70, 0x6c, 0x65, 0x45, 0x6e, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x50,
     0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x79, 0x0,  0x53, 0x61, 0x6d, 0x70,
     0x6c, 0x65, 0x49, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x72, 0x50, 0x72, 0x6f,
     0x70, 0x65, 0x72, 0x74, 0x79, 0x0,  0x53, 0x61, 0x6d, 0x70, 0x6c, 0x65,
     0x52, 0x65, 0x61, 0x6c, 0x50, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x79,
     0x0,  0x41, 0x6e, 0x6f, 0x74, 0x68, 0x65, 0x72, 0x42, 0x6f, 0x6f, 0x6c,
     0x65, 0x61, 0x6e, 0x0,  0x4c, 0x69, 0x6e, 0x6b, 0x53, 0x74, 0x61, 0x74,
     0x75, 0x73, 0x0,  0x4c, 0x69, 0x6e, 0x6b, 0x44, 0x6f, 0x77, 0x6e, 0x0,
     0x4c, 0x69, 0x6e, 0x6b, 0x55, 0x70, 0x0,  0x4e, 0x6f, 0x4c, 0x69, 0x6e,
     0x6b, 0x0,  0x0}};

/**
 * @brief Property names and sequence numbers in dummySimpleDict.
 * Order here is the same as the order in the dummySimpleDict.
 */
constexpr std::array<std::tuple<std::string_view, int>, 12> propertyNameSeq{{
    {"DummySimple", 0},
    {"ChildArrayProperty", 0},
    {"Id", 1},
    {"SampleEnabledProperty", 2},
    {"SampleIntegerProperty", 3},
    {"SampleRealProperty", 4},
    {"", 0},
    {"AnotherBoolean", 0},
    {"LinkStatus", 1},
    {"LinkDown", 0},
    {"LinkUp", 1},
    {"NoLink", 2},
}};

TEST(BejDictionaryTest, PropertyHeadOffsetTest)
{
    EXPECT_THAT(bejDictGetPropertyHeadOffset(), sizeof(BejDictionaryHeader));
}

TEST(BejDictionaryTest, AnnotationPropertyHeadOffsetTest)
{
    EXPECT_THAT(bejDictGetFirstAnnotatedPropertyOffset(),
                sizeof(BejDictionaryHeader) + sizeof(BejDictionaryProperty));
}

TEST(BejDictionaryTest, ValidPropertyTest)
{
    const struct BejDictionaryHeader* header =
        (const struct BejDictionaryHeader*)dummySimpleDict.data();
    uint16_t propHead = bejDictGetPropertyHeadOffset();
    // Read each property in the dictionary and verify that the property name is
    // correct.
    for (uint16_t index = 0; index < header->entryCount; ++index)
    {
        uint16_t offset = propHead + sizeof(BejDictionaryProperty) * index;
        const struct BejDictionaryProperty* property;
        EXPECT_THAT(bejDictGetProperty(dummySimpleDict.data(), offset,
                                       std::get<1>(propertyNameSeq[index]),
                                       &property),
                    0);
        EXPECT_THAT(
            bejDictGetPropertyName(dummySimpleDict.data(), property->nameOffset,
                                   property->nameLength),
            std::get<0>(propertyNameSeq[index]));
    }
}

TEST(BejDictionaryTest, invalidPropertyOffsetTest)
{
    const struct BejDictionaryProperty* property;
    EXPECT_THAT(bejDictGetProperty(dummySimpleDict.data(), /*offset=*/0,
                                   /*sequenceNumber=*/0, &property),
                bejErrorInvalidPropertyOffset);
    uint16_t propHead = bejDictGetPropertyHeadOffset();
    EXPECT_THAT(bejDictGetProperty(dummySimpleDict.data(), propHead + 1,
                                   /*sequenceNumber=*/0, &property),
                bejErrorInvalidPropertyOffset);
}

TEST(BejDictionaryTest, InvalidPropertyNameLength)
{
    // Make a copy of the dummySimpleDict.
    std::vector<uint8_t> modifiedDictionary = {dummySimpleDict.begin(),
                                               dummySimpleDict.end()};

    // Find a property and modify its nameLength to be out of bounds.
    struct BejDictionaryHeader* header =
        (struct BejDictionaryHeader*)modifiedDictionary.data();

    ASSERT_GE(header->entryCount, 1);
    struct BejDictionaryProperty* property =
        (struct BejDictionaryProperty*)(modifiedDictionary.data() +
                                        sizeof(BejDictionaryHeader));

    // Increase the nameLength to go beyond the dictionary size.
    property->nameLength = 255;

    // Now try to get this property by sequence number; it should fail due to
    // the invalid name length.
    const struct BejDictionaryProperty* retrievedProperty = nullptr;
    EXPECT_EQ(bejDictGetProperty(modifiedDictionary.data(),
                                 bejDictGetPropertyHeadOffset(),
                                 property->sequenceNumber, &retrievedProperty),
              bejErrorInvalidSize);
}

} // namespace libbej
