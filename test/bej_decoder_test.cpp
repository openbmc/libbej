#include "bej_common_test.hpp"
#include "bej_decoder_json.hpp"
#include "bej_encoder_json.hpp"

#include <memory>
#include <string_view>
#include <vector>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libbej
{

struct BejDecoderTestParams
{
    const std::string testName;
    const BejTestInputFiles inputFiles;
};

void PrintTo(const BejDecoderTestParams& params, std::ostream* os)
{
    *os << params.testName;
}

using BejDecoderTest = testing::TestWithParam<BejDecoderTestParams>;

const BejTestInputFiles driveOemTestFiles = {
    .jsonFile = "../test/json/drive_oem.json",
    .schemaDictionaryFile = "../test/dictionaries/drive_oem_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = "../test/encoded/drive_oem_enc.bin",
};

const BejTestInputFiles circuitTestFiles = {
    .jsonFile = "../test/json/circuit.json",
    .schemaDictionaryFile = "../test/dictionaries/circuit_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = "../test/encoded/circuit_enc.bin",
};

const BejTestInputFiles storageTestFiles = {
    .jsonFile = "../test/json/storage.json",
    .schemaDictionaryFile = "../test/dictionaries/storage_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = "../test/encoded/storage_enc.bin",
};

const BejTestInputFiles dummySimpleTestFiles = {
    .jsonFile = "../test/json/dummysimple.json",
    .schemaDictionaryFile = "../test/dictionaries/dummy_simple_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = "../test/encoded/dummy_simple_enc.bin",
};

TEST_P(BejDecoderTest, Decode)
{
    const BejDecoderTestParams& test_case = GetParam();
    auto inputsOrErr = loadInputs(test_case.inputFiles);
    EXPECT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .errorDictionary = inputsOrErr->errorDictionary,
    };

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, inputsOrErr->encodedStream), 0);
    std::string decoded = decoder.getOutput();
    nlohmann::json jsonDecoded = nlohmann::json::parse(decoded);

    // Just comparing nlohmann::json types could lead to errors. It compares the
    // byte values. So int64 and unit64 comparisons might be incorrect. Eg:
    // bytes values for -5 and 18446744073709551611 are the same. So compare the
    // string values.
    EXPECT_TRUE(jsonDecoded.dump() == inputsOrErr->expectedJson.dump());
}

TEST(BejDecoderSecurityTest, MaxOperationsLimit)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .errorDictionary = inputsOrErr->errorDictionary,
    };

    // Each array element below consists of a set and two properties, resulting
    // in 3 operations. 400,000 elements will result in 1,200,000 operations,
    // which should exceed the limit of 1,000,000.
    constexpr int numElements = 400000;

    auto root = std::make_unique<RedfishPropertyParent>();
    bejTreeInitSet(root.get(), "DummySimple");

    auto childArray = std::make_unique<RedfishPropertyParent>();
    bejTreeInitArray(childArray.get(), "ChildArrayProperty");
    bejTreeLinkChildToParent(root.get(), childArray.get());

    std::vector<std::unique_ptr<RedfishPropertyParent>> sets;
    std::vector<std::unique_ptr<RedfishPropertyLeafBool>> bools;
    std::vector<std::unique_ptr<RedfishPropertyLeafEnum>> enums;
    sets.reserve(numElements);
    bools.reserve(numElements);
    enums.reserve(numElements);

    for (int i = 0; i < numElements; ++i)
    {
        auto chArraySet = std::make_unique<RedfishPropertyParent>();
        bejTreeInitSet(chArraySet.get(), nullptr);

        auto chArraySetBool = std::make_unique<RedfishPropertyLeafBool>();
        bejTreeAddBool(chArraySet.get(), chArraySetBool.get(), "AnotherBoolean",
                       true);

        auto chArraySetLs = std::make_unique<RedfishPropertyLeafEnum>();
        bejTreeAddEnum(chArraySet.get(), chArraySetLs.get(), "LinkStatus",
                       "NoLink");

        bejTreeLinkChildToParent(childArray.get(), chArraySet.get());

        sets.push_back(std::move(chArraySet));
        bools.push_back(std::move(chArraySetBool));
        enums.push_back(std::move(chArraySetLs));
    }

    libbej::BejEncoderJson encoder;
    encoder.encode(&dictionaries, bejMajorSchemaClass, root.get());
    std::vector<uint8_t> outputBuffer = encoder.getOutput();

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(outputBuffer)),
                bejErrorNotSuppoted);
}

/**
 * TODO: Add more test cases.
 * - Test Enums inside array elemets
 * - Array inside an array: is this a valid case?
 * - Real numbers with exponent part
 * - Every type inside an array.
 */
INSTANTIATE_TEST_SUITE_P(
    , BejDecoderTest,
    testing::ValuesIn<BejDecoderTestParams>({
        {"DriveOEM", driveOemTestFiles},
        {"Circuit", circuitTestFiles},
        {"Storage", storageTestFiles},
        {"DummySimple", dummySimpleTestFiles},
    }),
    [](const testing::TestParamInfo<BejDecoderTest::ParamType>& info) {
        return info.param.testName;
    });

} // namespace libbej
