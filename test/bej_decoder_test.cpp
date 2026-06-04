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

// Build a BejDictionaries view over already-loaded test inputs.
BejDictionaries makeDictionaries(const BejTestInputs& inputs)
{
    return BejDictionaries{
        .schemaDictionary = inputs.schemaDictionary,
        .schemaDictionarySize = inputs.schemaDictionarySize,
        .annotationDictionary = inputs.annotationDictionary,
        .annotationDictionarySize = inputs.annotationDictionarySize,
        .errorDictionary = inputs.errorDictionary,
        .errorDictionarySize = inputs.errorDictionarySize,
    };
}

TEST_P(BejDecoderTest, Decode)
{
    const BejDecoderTestParams& test_case = GetParam();
    auto inputsOrErr = loadInputs(test_case.inputFiles);
    EXPECT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
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

/**
 * TODO: Add more test cases.
 * - Test Enums inside array elements
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

TEST(BejDecoderSecurityTest, MaxOperationsLimit)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
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
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
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
    size_t zeroCountOffset = zeroCountPtr - outputBuffer.data();
    // nnint value for 101
    outputBuffer[zeroCountOffset + 1] = 101;

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(outputBuffer)),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, StringTooLong)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    auto root = std::make_unique<RedfishPropertyParent>();
    bejTreeInitSet(root.get(), "DummySimple");

    // Create a string with a length greater than MAX_BEJ_STRING_LEN (65536).
    std::string longString(65537, 'A');

    auto stringProp = std::make_unique<RedfishPropertyLeafString>();
    bejTreeAddString(root.get(), stringProp.get(), "Id", longString.c_str());

    libbej::BejEncoderJson encoder;
    encoder.encode(&dictionaries, bejMajorSchemaClass, root.get());
    std::vector<uint8_t> outputBuffer = encoder.getOutput();

    // The decoder should return an error because the string is too long.
    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(outputBuffer)),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, ValueBeyondStreamLength)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    auto root = std::make_unique<RedfishPropertyParent>();
    bejTreeInitSet(root.get(), "DummySimple");

    auto intProp = std::make_unique<RedfishPropertyLeafInt>();
    bejTreeAddInteger(root.get(), intProp.get(), "SampleIntegerProperty", 123);

    libbej::BejEncoderJson encoder;
    encoder.encode(&dictionaries, bejMajorSchemaClass, root.get());
    std::vector<uint8_t> outputBuffer = encoder.getOutput();

    // Tamper with the encoded stream to simulate a value extending beyond the
    // stream length. This stream only has an integer 0x7b. The value before it
    // is the length tuple.
    outputBuffer[outputBuffer.size() - 2] = 0x05;

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(outputBuffer)),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, MalformedRootSequenceSizeTooLarge)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);
    BejDictionaries dictionaries = makeDictionaries(*inputsOrErr);

    // byte[0]=0xFF claims a 255-byte sequence number, far past this 5-byte
    // buffer; must be rejected before bejGetNnint reads the sequence value.
    std::vector<uint8_t> malformed = {0xFF, 0x00, 0x00, 0x00, 0x00};

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(malformed)),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, MalformedRootValueLengthSizeTooLarge)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);
    BejDictionaries dictionaries = makeDictionaries(*inputsOrErr);

    // seq size=1, seq=0, format=0, then byte[3]=0xFF claims a 255-byte value
    // length that runs past this 5-byte buffer.
    std::vector<uint8_t> malformed = {0x01, 0x00, 0x00, 0xFF, 0x00};

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(malformed)),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, MalformedRootHeaderTruncated)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);
    BejDictionaries dictionaries = makeDictionaries(*inputsOrErr);

    // byte[0]=0x0A pushes the value-length size byte to index 12, past this
    // 6-byte buffer, even though it clears the minimum-root-size check.
    std::vector<uint8_t> malformed = {0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(malformed)),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, InvalidSchemaDictionarySize)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = 10,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, inputsOrErr->encodedStream),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, InvalidAnnotationDictionarySize)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = 10,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, inputsOrErr->encodedStream),
                bejErrorInvalidSize);
}

TEST(BejDecoderSecurityTest, InvalidErrorDictionarySize)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = 10,
    };

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, inputsOrErr->encodedStream),
                bejErrorInvalidSize);
}

TEST(BejDecoderResourceLinkTest, DecodeResourceLink)
{
    // Test that ResourceLink is decoded as "%L<ID>" format.
    // Manually craft a BEJ encoded stream with a ResourceLink property.
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    // Manually construct BEJ encoded stream:
    // Format byte = principalDataType << 4, so bejResourceLink(14) = 0xE0
    //
    // Structure:
    // - PLDM header (7 bytes)
    // - Root Set: S(2) + F(1) + L(2) + V(9) where V = count(2) + child(7)
    // - Child ResourceLink: S(2) + F(1) + L(2) + V(2) = 7 bytes
    std::vector<uint8_t> encodedStream = {
        // PLDM header (7 bytes)
        0x00,
        0xF0,
        0xF0,
        0xF1, // bejVersion (0xF1F0F000 little endian)
        0x00,
        0x00, // reserved
        0x00, // schemaClass (major)
        // Root Set (DummySimple, seq=0)
        0x01,
        0x00, // S: nnint(0) -> seq=0
        0x00, // F: bejSet (0 << 4 = 0x00)
        0x01,
        0x09, // L: nnint(9) -> value is 9 bytes
        // Set value: element count + child SFLV
        0x01,
        0x01, // element count: nnint(1) -> 1 child
        // Child ResourceLink (Id, seq=1)
        0x01,
        0x02, // S: nnint(2) -> seq=1, schema=primary
        0xE0, // F: bejResourceLink (14 << 4 = 0xE0)
        0x01,
        0x02, // L: nnint(2) -> value is 2 bytes
        0x01,
        0x2A, // V: nnint(42) -> PDR ID = 42
    };

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(encodedStream)), 0);

    std::string decoded = decoder.getOutput();
    nlohmann::json jsonDecoded = nlohmann::json::parse(decoded);

    // Verify the ResourceLink is decoded as "%L42"
    EXPECT_TRUE(jsonDecoded.contains("Id"));
    EXPECT_EQ(jsonDecoded["Id"].get<std::string>(), "%L42");
}

TEST(BejDecoderResourceLinkTest, DecodeResourceLinkNull)
{
    // Test that ResourceLink with zero length is decoded as null.
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    // Same structure but with zero-length value (null ResourceLink)
    // Child SFLV with L=0: S(2) + F(1) + L(2) + V(0) = 5 bytes
    // Set value: count(2) + child(5) = 7 bytes
    std::vector<uint8_t> encodedStream = {
        // PLDM header (7 bytes)
        0x00,
        0xF0,
        0xF0,
        0xF1, // bejVersion
        0x00,
        0x00, // reserved
        0x00, // schemaClass
        // Root Set (DummySimple, seq=0)
        0x01,
        0x00, // S: nnint(0) -> seq=0
        0x00, // F: bejSet
        0x01,
        0x07, // L: nnint(7) -> value is 7 bytes
        // Set value: element count + child SFLV
        0x01,
        0x01, // element count: nnint(1) -> 1 child
        // Child ResourceLink (Id, seq=1) with null value
        0x01,
        0x02, // S: nnint(2) -> seq=1
        0xE0, // F: bejResourceLink (14 << 4 = 0xE0)
        0x01,
        0x00, // L: nnint(0) -> zero length means null
    };

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(encodedStream)), 0);

    std::string decoded = decoder.getOutput();
    nlohmann::json jsonDecoded = nlohmann::json::parse(decoded);

    // Verify the ResourceLink is decoded as null
    EXPECT_TRUE(jsonDecoded.contains("Id"));
    EXPECT_TRUE(jsonDecoded["Id"].is_null());
}

TEST(BejDecoderPayloadLengthTest, IgnoresTrailingBytes)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    // Decode the reference (un-padded) stream so we can compare against it.
    BejDecoderJson refDecoder;
    ASSERT_EQ(refDecoder.decode(dictionaries, inputsOrErr->encodedStream), 0);
    std::string refOutput = refDecoder.getOutput();

    // The decoder must derive the payload length from the root SFLV alone,
    // so its output has to be byte-identical to the unpadded decoding
    // regardless of how much padding is appended or what byte pattern it
    // uses. Cover the common real-world cases (zero padding from a
    // fixed-size PLDM buffer, 0xFF sentinel, arbitrary pattern) at several
    // sizes to make the equivalence robust.
    const std::vector<std::pair<size_t, uint8_t>> paddings = {
        {1, 0x00}, {8, 0x00}, {64, 0x00}, // zero padding (typical PLDM)
        {1, 0xFF}, {8, 0xFF}, {64, 0xFF}, // sentinel padding
        {1, 0xA5}, {8, 0xA5},             // arbitrary pattern
    };

    for (const auto& [padSize, padByte] : paddings)
    {
        std::vector<uint8_t> padded(inputsOrErr->encodedStream.begin(),
                                    inputsOrErr->encodedStream.end());
        padded.insert(padded.end(), padSize, padByte);

        BejDecoderJson decoder;
        decoder.setTrailingDataPolicy(bejTrailingIgnore);
        EXPECT_EQ(decoder.decode(dictionaries, std::span(padded)), 0)
            << "padSize=" << padSize << " padByte=" << int(padByte);
        EXPECT_EQ(decoder.getOutput(), refOutput)
            << "padSize=" << padSize << " padByte=" << int(padByte);
    }
}

TEST(BejDecoderPayloadLengthTest, RejectsBufferShorterThanRootTuple)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    // Truncate the buffer mid-payload. The root SFLV claims a value length
    // that no longer fits, so the decoder must reject the input upfront
    // rather than partially decoding it.
    std::span<const uint8_t> truncated(inputsOrErr->encodedStream.data(),
                                       inputsOrErr->encodedStream.size() - 1);

    BejDecoderJson decoder;
    EXPECT_EQ(decoder.decode(dictionaries, truncated), bejErrorInvalidSize);
}

TEST(BejDecoderPayloadLengthTest, IgnoresTrailingBytesByDefault)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    BejDecoderJson refDecoder;
    ASSERT_EQ(refDecoder.decode(dictionaries, inputsOrErr->encodedStream), 0);
    std::string refOutput = refDecoder.getOutput();

    std::vector<uint8_t> padded(inputsOrErr->encodedStream.begin(),
                                inputsOrErr->encodedStream.end());
    padded.insert(padded.end(), {0xFF, 0xFF, 0xFF, 0xFF});

    // The default policy is now bejTrailingIgnore, so a buffer with bytes
    // past the root SFLV's value length must decode successfully without an
    // explicit policy being set, matching the unpadded decoding.
    BejDecoderJson decoder;
    EXPECT_EQ(decoder.decode(dictionaries, std::span(padded)), 0);
    EXPECT_EQ(decoder.getOutput(), refOutput);
}

TEST(BejDecoderPayloadLengthTest, WarnsOnTrailingBytes)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    BejDecoderJson refDecoder;
    ASSERT_EQ(refDecoder.decode(dictionaries, inputsOrErr->encodedStream), 0);
    std::string refOutput = refDecoder.getOutput();

    std::vector<uint8_t> padded(inputsOrErr->encodedStream.begin(),
                                inputsOrErr->encodedStream.end());
    padded.insert(padded.end(), {0xFF, 0xFF, 0xFF, 0xFF});

    // Warn policy must still decode successfully and produce the same
    // output as the unpadded reference, while emitting a diagnostic to
    // stderr that mentions the trailing bytes.
    BejDecoderJson decoder;
    decoder.setTrailingDataPolicy(bejTrailingWarn);

    testing::internal::CaptureStderr();
    EXPECT_EQ(decoder.decode(dictionaries, std::span(padded)), 0);
    std::string captured = testing::internal::GetCapturedStderr();

    EXPECT_EQ(decoder.getOutput(), refOutput);
    EXPECT_THAT(captured, testing::HasSubstr("trailing bytes after root SFLV"));
    EXPECT_THAT(captured, testing::HasSubstr("ignored"));
}

TEST(BejDecoderPayloadLengthTest, ErrorsOnTrailingBytes)
{
    auto inputsOrErr = loadInputs(dummySimpleTestFiles);
    ASSERT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .schemaDictionarySize = inputsOrErr->schemaDictionarySize,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .annotationDictionarySize = inputsOrErr->annotationDictionarySize,
        .errorDictionary = inputsOrErr->errorDictionary,
        .errorDictionarySize = inputsOrErr->errorDictionarySize,
    };

    std::vector<uint8_t> padded(inputsOrErr->encodedStream.begin(),
                                inputsOrErr->encodedStream.end());
    padded.insert(padded.end(), {0xFF, 0xFF, 0xFF, 0xFF});

    // Explicit error policy rejects the input as bejErrorInvalidSize.
    BejDecoderJson strict;
    strict.setTrailingDataPolicy(bejTrailingError);
    EXPECT_EQ(strict.decode(dictionaries, std::span(padded)),
              bejErrorInvalidSize);
}

} // namespace libbej
