#include "nlohmann/json.hpp"

#include <stdint.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>

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
    nlohmann::json expectedJson;
    const uint8_t* schemaDictionary;
    const uint8_t* annotationDictionary;
    const uint8_t* errorDictionary;
    std::span<const uint8_t> encodedStream;
};

class BejFileReader
{
  public:
    static std::optional<BejTestInputs>
        loadInputs(const BejTestInputFiles& files,
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
        std::streamsize encLen = 0;
        if (files.encodedStreamFile != nullptr)
        {
            encLen = readBinaryFile(files.encodedStreamFile,
                                    std::span(encBuffer, maxBufferSize));
            if (encLen == 0)
            {
                return std::nullopt;
            }
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

  private:
    // Buffer size for storing a single binary file data.
    static constexpr uint32_t maxBufferSize = 16 * 1024;

    static std::streamsize readBinaryFile(const char* fileName,
                                          std::span<uint8_t> buffer)
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
};

} // namespace libbej
