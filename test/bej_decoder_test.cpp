#include "bej_decoder_json.hpp"
#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libbej
{

struct BejTestInputFiles
{
    const char* jsonFile;
    const char* schemaDictionaryFile;
    const char* annotationDictionaryFile;
    const char* errorDictionaryFile;
    const char* encodedStreamFile;
};

struct BejTestInputs
{
    const nlohmann::json expectedJson;
    const uint8_t* schemaDictionary;
    const uint8_t* annotationDictionary;
    const uint8_t* errorDictionary;
    std::span<const uint8_t> encodedStream;
};

struct BejDecoderTestParams
{
    const std::string testName;
    const BejTestInputFiles inputFiles;
};

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

// Buffer size for storing a single binary file data.
constexpr uint32_t maxBufferSize = 16 * 1024;

std::streamsize readBinaryFile(const char* fileName, std::span<uint8_t> buffer)
{
    std::ifstream inputStream(fileName, std::ios::binary);
    if (!inputStream.is_open())
    {
        std::cerr << "Cannot open file: " << fileName << "\n";
        return 0;
    }
    auto readLength = inputStream.readsome(
        reinterpret_cast<char*>(buffer.data()), buffer.size_bytes());
    if (inputStream.peek() != EOF)
    {
        std::cerr << "Failed to read the complete file: " << fileName
                  << "  read length: " << readLength << "\n";
        return 0;
    }
    return readLength;
}

std::optional<BejTestInputs> loadInputs(const BejTestInputFiles& files,
                                        bool readErrorDictionary = false)
{
    std::ifstream jsonInput(files.jsonFile);
    if (!jsonInput.is_open())
    {
        std::cerr << "Cannot open file: " << files.jsonFile << "\n";
        return std::nullopt;
    }
    nlohmann::json expJson;
    jsonInput >> expJson;

    static uint8_t schemaDictBuffer[maxBufferSize];
    if (readBinaryFile(files.schemaDictionaryFile,
                       std::span(schemaDictBuffer, maxBufferSize)) == 0)
    {
        return std::nullopt;
    }

    static uint8_t annoDictBuffer[maxBufferSize];
    if (readBinaryFile(files.annotationDictionaryFile,
                       std::span(annoDictBuffer, maxBufferSize)) == 0)
    {
        return std::nullopt;
    }

    static uint8_t encBuffer[maxBufferSize];
    auto encLen = readBinaryFile(files.encodedStreamFile,
                                 std::span(encBuffer, maxBufferSize));
    if (encLen == 0)
    {
        return std::nullopt;
    }

    static uint8_t errorDict[maxBufferSize];
    if (readErrorDictionary)
    {
        if (readBinaryFile(files.errorDictionaryFile,
                           std::span(errorDict, maxBufferSize)) == 0)
        {
            return std::nullopt;
        }
    }

    BejTestInputs inputs = {
        .expectedJson = expJson,
        .schemaDictionary = schemaDictBuffer,
        .annotationDictionary = annoDictBuffer,
        .errorDictionary = errorDict,
        .encodedStream = std::span(encBuffer, encLen),
    };
    return inputs;
}

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
