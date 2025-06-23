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
                bejErrorNotSupported);
}

TEST(BejDecoderSecurityTest, RealWithTooManyLeadingZeros)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .errorDictionary = inputsOrErr->errorDictionary,
    };

    auto root = std::make_unique<RedfishPropertyParent>();
    bejTreeInitSet(root.get(), "DummySimple");

    // 1.003 was randomely chosen
    auto real = std::make_unique<RedfishPropertyLeafReal>();
    bejTreeAddReal(root.get(), real.get(), "SampleRealProperty", 1.003);

    libbej::BejEncoderJson encoder;
    encoder.encode(&dictionaries, bejMajorSchemaClass, root.get());
    std::vector<uint8_t> outputBuffer = encoder.getOutput();

    // Manually tamper with the encoded stream to create the attack vector.
    // We will find the `bejReal` property and overwrite its `zeroCount`.
    // The property "SampleRealProperty" has sequence number 4. The encoded
    // sequence number is `(4 << 1) | 0 = 8`. The nnint for 8 is `0x01, 0x08`.
    const std::vector<uint8_t> realPropSeqNum = {0x01, 0x08};
    auto it = std::search(outputBuffer.begin(), outputBuffer.end(),
                          realPropSeqNum.begin(), realPropSeqNum.end());
    ASSERT_NE(it, outputBuffer.end()) << "Could not find bejReal property";

    // The structure of a bejReal SFLV is: S(nnint) F(u8) L(nnint) V(...)
    // The structure of V is: nnint(len(whole)), int(whole), nnint(zeroCount)...
    // Find the start of the value (V) by skipping S, F, and L.
    size_t sflvOffset = std::distance(outputBuffer.begin(), it);
    const uint8_t* streamPtr = outputBuffer.data() + sflvOffset;
    // Skip S
    streamPtr += bejGetNnintSize(streamPtr);
    // Skip F
    streamPtr++;
    // Skip L
    const uint8_t* valuePtr = streamPtr + bejGetNnintSize(streamPtr);

    // Find the start of zeroCount within V.
    const uint8_t* zeroCountPtr = valuePtr + bejGetNnintSize(valuePtr);
    zeroCountPtr += bejGetNnint(valuePtr); // Skip int(whole)

    // The original zeroCount for 1.003 is 2. nnint(2) is `0x01, 0x02`.
    // We replace it with a zeroCount of 101, which exceeds the limit.
    // nnint(101) is `0x01, 101`. The size is the same (2 bytes), so we don't
    // need to update the SFLV length field (L).
    ASSERT_EQ(bejGetNnint(zeroCountPtr), 2);
    size_t zeroCountOffset = std::distance(outputBuffer.data(), zeroCountPtr);
    // nnint value for 101
    outputBuffer[zeroCountOffset + 1] = 101;

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(outputBuffer)),
                bejErrorInvalidSize);
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
