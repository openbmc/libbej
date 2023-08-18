#include "bej_dictionary.h"
#include "bej_encoder_core.h"
#include "bej_tree.h"

#include "bej_decoder_json.hpp"
#include "bej_load_files.hpp"

#include <vector>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libbej
{

struct BejEncoderTestParams
{
    const std::string testName;
    const BejTestInputFiles inputFiles;
    std::string expectedJson;
    struct RedfishPropertyParent* (*createResource)();
};

using BejEncoderTest = testing::TestWithParam<BejEncoderTestParams>;

const BejTestInputFiles dummySimpleTestFiles = {
    .jsonFile = "../test/json/dummysimple.json",
    .schemaDictionaryFile = "../test/dictionaries/dummy_simple_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = "../test/encoded/dummy_simple_enc.bin",
};

const BejTestInputFiles driveOemTestFiles = {
    .jsonFile = "../test/json/drive_oem.json",
    .schemaDictionaryFile = "../test/dictionaries/drive_oem_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = "../test/encoded/drive_oem_enc.bin",
};

const BejTestInputFiles chassisTestFiles = {
    .jsonFile = "../test/json/chassis.json",
    .schemaDictionaryFile = "../test/dictionaries/chassis_dict.bin",
    .annotationDictionaryFile = "../test/dictionaries/annotation_dict.bin",
    .errorDictionaryFile = "",
    .encodedStreamFile = nullptr,
};

int recvOutput(const void* data, size_t data_size, void* handlerContext)
{
    auto stack = reinterpret_cast<std::vector<uint8_t>*>(handlerContext);
    const uint8_t* dataBuf = reinterpret_cast<const uint8_t*>(data);
    stack->insert(stack->end(), dataBuf, dataBuf + data_size);
    return 0;
}

bool stackEmpty(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    return stack->empty();
}

void* stackPeek(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    if (stack->empty())
    {
        return nullptr;
    }
    return stack->back();
}

void* stackPop(void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    if (stack->empty())
    {
        return nullptr;
    }
    void* value = stack->back();
    stack->pop_back();
    return value;
}

int stackPush(void* p, void* dataPtr)
{
    auto stack = reinterpret_cast<std::vector<void*>*>(dataPtr);
    stack->push_back(p);
    return 0;
}

struct RedfishPropertyParent* createDummyResource()
{
    static struct RedfishPropertyParent root;
    bejTreeInitSet(&root, "DummySimple");

    static struct RedfishPropertyLeafString id;
    bejTreeAddString(&root, &id, "Id", "Dummy ID");

    static struct RedfishPropertyLeafInt intProp;
    bejTreeAddInteger(&root, &intProp, "SampleIntegerProperty", -5);

    static struct RedfishPropertyLeafReal real;
    bejTreeAddReal(&root, &real, "SampleRealProperty", -5576.90001);

    static struct RedfishPropertyLeafNull enProp;
    bejTreeAddNull(&root, &enProp, "SampleEnabledProperty");

    static struct RedfishPropertyParent chArraySet1;
    bejTreeInitSet(&chArraySet1, nullptr);

    static struct RedfishPropertyLeafBool chArraySet1bool;
    bejTreeAddBool(&chArraySet1, &chArraySet1bool, "AnotherBoolean", true);

    static struct RedfishPropertyLeafEnum chArraySet1Ls;
    bejTreeAddEnum(&chArraySet1, &chArraySet1Ls, "LinkStatus", "NoLink");

    static struct RedfishPropertyParent chArraySet2;
    bejTreeInitSet(&chArraySet2, nullptr);

    static struct RedfishPropertyLeafEnum chArraySet2Ls;
    bejTreeAddEnum(&chArraySet2, &chArraySet2Ls, "LinkStatus", "LinkDown");

    static struct RedfishPropertyParent chArray;
    bejTreeInitArray(&chArray, "ChildArrayProperty");

    bejTreeLinkChildToParent(&chArray, &chArraySet1);
    bejTreeLinkChildToParent(&chArray, &chArraySet2);

    bejTreeLinkChildToParent(&root, &chArray);
    return &root;
}

const std::string driveOemJson = R"(
      {
        "@odata.id": "/redfish/v1/drives/1",
        "@odata.type": "#Drive.v1_5_0.Drive",
        "Id": "Drive1",
        "Actions": {
            "#Drive.Reset": {
                "target": "/redfish/v1/drives/1/Actions/Drive.Reset",
                "title": "Reset a Drive",
                "ResetType@Redfish.AllowableValues": [
                        "On",
                        "ForceOff",
                        "ForceRestart",
                        "Nmi",
                        "ForceOn",
                        "PushPowerButton"
                    ]
            }
        },
        "Status@Message.ExtendedInfo": [
            {
                "MessageId": "PredictiveFailure",
                "RelatedProperties": ["FailurePredicted", "MediaType"]
            }
        ],
        "Identifiers": [],
        "Links": {}
    }
)";

struct RedfishPropertyParent* createDriveOem()
{
    static struct RedfishPropertyParent root;
    bejTreeInitSet(&root, "Drive");

    static struct RedfishPropertyLeafString odataId;
    bejTreeAddString(&root, &odataId, "@odata.id", "/redfish/v1/drives/1");

    static struct RedfishPropertyLeafString odataType;
    bejTreeAddString(&root, &odataType, "@odata.type", "#Drive.v1_5_0.Drive");

    static struct RedfishPropertyLeafString id;
    bejTreeAddString(&root, &id, "Id", "Drive1");

    static struct RedfishPropertyParent actions;
    bejTreeInitSet(&actions, "Actions");

    static struct RedfishPropertyParent drRst;
    bejTreeInitSet(&drRst, "#Drive.Reset");

    static struct RedfishPropertyLeafString drRstTarget;
    bejTreeAddString(&drRst, &drRstTarget, "target",
                     "/redfish/v1/drives/1/Actions/Drive.Reset");

    static struct RedfishPropertyLeafString drRstTitle;
    bejTreeAddString(&drRst, &drRstTitle, "title", "Reset a Drive");

    static struct RedfishPropertyParent drRstType;
    bejTreeInitPropertyAnnotated(&drRstType, "ResetType");

    static struct RedfishPropertyParent drRstTypeAllowable;
    bejTreeInitArray(&drRstTypeAllowable, "@Redfish.AllowableValues");

    static struct RedfishPropertyLeafString drRstTypeAllowableS1;
    bejTreeAddString(&drRstTypeAllowable, &drRstTypeAllowableS1, "", "On");

    static struct RedfishPropertyLeafString drRstTypeAllowableS2;
    bejTreeAddString(&drRstTypeAllowable, &drRstTypeAllowableS2, "",
                     "ForceOff");

    static struct RedfishPropertyLeafString drRstTypeAllowableS3;
    bejTreeAddString(&drRstTypeAllowable, &drRstTypeAllowableS3, "",
                     "ForceRestart");

    static struct RedfishPropertyLeafString drRstTypeAllowableS4;
    bejTreeAddString(&drRstTypeAllowable, &drRstTypeAllowableS4, "", "Nmi");

    static struct RedfishPropertyLeafString drRstTypeAllowableS5;
    bejTreeAddString(&drRstTypeAllowable, &drRstTypeAllowableS5, "", "ForceOn");

    static struct RedfishPropertyLeafString drRstTypeAllowableS6;
    bejTreeAddString(&drRstTypeAllowable, &drRstTypeAllowableS6, "",
                     "PushPowerButton");

    bejTreeLinkChildToParent(&drRstType, &drRstTypeAllowable);
    bejTreeLinkChildToParent(&drRst, &drRstType);
    bejTreeLinkChildToParent(&actions, &drRst);
    bejTreeLinkChildToParent(&root, &actions);

    static struct RedfishPropertyParent statusAnt;
    bejTreeInitPropertyAnnotated(&statusAnt, "Status");

    static struct RedfishPropertyParent statusAntMsgExtInfo;
    bejTreeInitArray(&statusAntMsgExtInfo, "@Message.ExtendedInfo");

    static struct RedfishPropertyParent statusAntMsgExtInfoSet1;
    bejTreeInitSet(&statusAntMsgExtInfoSet1, nullptr);

    static struct RedfishPropertyLeafString statusAntMsgExtInfoSet1P1;
    bejTreeAddString(&statusAntMsgExtInfoSet1, &statusAntMsgExtInfoSet1P1,
                     "MessageId", "PredictiveFailure");

    static struct RedfishPropertyParent statusAntMsgExtInfoSet1P2;
    bejTreeInitArray(&statusAntMsgExtInfoSet1P2, "RelatedProperties");
    bejTreeLinkChildToParent(&statusAntMsgExtInfoSet1,
                             &statusAntMsgExtInfoSet1P2);

    static struct RedfishPropertyLeafString statusAntMsgExtInfoSet1P2Ele1;
    bejTreeAddString(&statusAntMsgExtInfoSet1P2, &statusAntMsgExtInfoSet1P2Ele1,
                     "", "FailurePredicted");

    static struct RedfishPropertyLeafString statusAntMsgExtInfoSet1P2Ele2;
    bejTreeAddString(&statusAntMsgExtInfoSet1P2, &statusAntMsgExtInfoSet1P2Ele2,
                     "", "MediaType");

    bejTreeLinkChildToParent(&statusAntMsgExtInfo, &statusAntMsgExtInfoSet1);
    bejTreeLinkChildToParent(&statusAnt, &statusAntMsgExtInfo);
    bejTreeLinkChildToParent(&root, &statusAnt);

    static struct RedfishPropertyParent identifiers;
    bejTreeInitArray(&identifiers, "Identifiers");
    bejTreeLinkChildToParent(&root, &identifiers);

    static struct RedfishPropertyParent links;
    bejTreeInitSet(&links, "Links");
    bejTreeLinkChildToParent(&root, &links);

    return &root;
}

/**
 * @brief Storage for an array of links with an annotated odata.count.
 *
 * This doesn't contain storage for the link itself.
 *
 * Eg:
 * "Contains": [],
 * "Contains@odata.count": 0,
 */
struct RedfishArrayOfLinksJson
{
    struct RedfishPropertyParent array;
    struct RedfishPropertyParent annotatedProperty;
    struct RedfishPropertyLeafInt count;
};

/**
 * @brief Storage for a single odata.id link inside a JSON "Set" object.
 *
 * Eg: FieldName: {
 *   "@odata.id": "/redfish/v1/Chassis/Something"
 * }
 */
struct RedfishLinkJson
{
    struct RedfishPropertyParent set;
    struct RedfishPropertyLeafString odataId;
};

void addLinkToTree(struct RedfishPropertyParent* parent,
                   struct RedfishPropertyParent* linkSet,
                   const char* linkSetLabel,
                   struct RedfishPropertyLeafString* odataId,
                   const char* linkValue)
{
    bejTreeInitSet(linkSet, linkSetLabel);
    bejTreeAddString(linkSet, odataId, "@odata.id", linkValue);
    bejTreeLinkChildToParent(parent, linkSet);
}

void redfishCreateArrayOfLinksJson(struct RedfishPropertyParent* parent,
                                   const char* arrayName, int linkCount,
                                   const char* const links[],
                                   struct RedfishArrayOfLinksJson* linksInfo,
                                   struct RedfishLinkJson* linkJsonArray)
{
    bejTreeInitArray(&linksInfo->array, arrayName);
    bejTreeLinkChildToParent(parent, &linksInfo->array);

    bejTreeInitPropertyAnnotated(&linksInfo->annotatedProperty, arrayName);
    bejTreeAddInteger(&linksInfo->annotatedProperty, &linksInfo->count,
                      "@odata.count", linkCount);
    bejTreeLinkChildToParent(parent, &linksInfo->annotatedProperty);

    for (int i = 0; i < linkCount; ++i)
    {
        addLinkToTree(&linksInfo->array, &linkJsonArray[i].set, NULL,
                      &linkJsonArray[i].odataId, links[i]);
    }
}

struct RedfishPropertyParent* createChassisResource()
{
    constexpr int containsLinkCount = 2;
    const char* contains[containsLinkCount] = {"/redfish/v1/Chassis/Disk_0",
                                               "/redfish/v1/Chassis/Disk_1"};
    const char* storage[1] = {"/redfish/v1/Systems/system/Storage/SATA"};
    const char* drives[1] = {"/redfish/v1/Chassis/SomeChassis/Drives/SATA_0"};
    static struct RedfishPropertyParent root;
    static struct RedfishPropertyLeafString odataId;
    static struct RedfishPropertyParent links;
    static struct RedfishPropertyParent computerSystemsArray;
    static struct RedfishPropertyParent computerSystemsLinkSet;
    static struct RedfishPropertyLeafString computerSystemsLinkOdataId;
    static struct RedfishPropertyParent containedBySet;
    static struct RedfishPropertyLeafString containedByOdataId;
    static struct RedfishArrayOfLinksJson containsArray;
    static struct RedfishLinkJson containsLinks[containsLinkCount];
    static struct RedfishArrayOfLinksJson storageArray;
    static struct RedfishLinkJson storageLink;
    static struct RedfishArrayOfLinksJson drives_array;
    static struct RedfishLinkJson drive_link;

    bejTreeInitSet(&root, "Chassis");
    bejTreeAddString(&root, &odataId, "@odata.id",
                     "/redfish/v1/Chassis/SomeChassis");

    bejTreeInitSet(&links, "Links");
    bejTreeLinkChildToParent(&root, &links);

    bejTreeInitArray(&computerSystemsArray, "ComputerSystems");
    bejTreeLinkChildToParent(&links, &computerSystemsArray);

    addLinkToTree(&computerSystemsArray, &computerSystemsLinkSet, "",
                  &computerSystemsLinkOdataId, "/redfish/v1/Systems/system");

    addLinkToTree(&links, &containedBySet, "ContainedBy", &containedByOdataId,
                  "/redfish/v1/Chassis/SomeOtherChassis");

    redfishCreateArrayOfLinksJson(&links, "Contains", containsLinkCount,
                                  contains, &containsArray, containsLinks);

    redfishCreateArrayOfLinksJson(&links, "Storage", /*linkCount=*/1, storage,
                                  &storageArray, &storageLink);

    redfishCreateArrayOfLinksJson(&links, "Drives", /*linkCount=*/1, drives,
                                  &drives_array, &drive_link);

    return &root;
}

TEST_P(BejEncoderTest, Encode)
{
    const BejEncoderTestParams& test_case = GetParam();
    auto inputsOrErr = BejFileReader::loadInputs(test_case.inputFiles);
    EXPECT_TRUE(inputsOrErr);

    BejDictionaries dictionaries = {
        .schemaDictionary = inputsOrErr->schemaDictionary,
        .annotationDictionary = inputsOrErr->annotationDictionary,
        .errorDictionary = inputsOrErr->errorDictionary,
    };

    std::vector<uint8_t> outputBuffer;
    struct BejEncoderOutputHandler output = {
        .handlerContext = &outputBuffer,
        .recvOutput = &recvOutput,
    };

    std::vector<void*> pointerStack;
    struct BejPointerStackCallback stackCallbacks = {
        .stackContext = &pointerStack,
        .stackEmpty = stackEmpty,
        .stackPeek = stackPeek,
        .stackPop = stackPop,
        .stackPush = stackPush,
        .deleteStack = NULL,
    };

    bejEncode(&dictionaries, BEJ_DICTIONARY_START_AT_HEAD, bejMajorSchemaClass,
              test_case.createResource(), &output, &stackCallbacks);

    BejDecoderJson decoder;
    EXPECT_THAT(decoder.decode(dictionaries, std::span(outputBuffer)), 0);
    std::string decoded = decoder.getOutput();
    nlohmann::json jsonDecoded = nlohmann::json::parse(decoded);

    if (!test_case.expectedJson.empty())
    {
        inputsOrErr->expectedJson =
            nlohmann::json::parse(test_case.expectedJson);
    }
    EXPECT_TRUE(jsonDecoded.dump() == inputsOrErr->expectedJson.dump());
}

/**
 * TODO: Add more test cases.
 */
INSTANTIATE_TEST_SUITE_P(
    , BejEncoderTest,
    testing::ValuesIn<BejEncoderTestParams>({
        {"DriveOEM", driveOemTestFiles, driveOemJson, &createDriveOem},
        {"DummySimple", dummySimpleTestFiles, "", &createDummyResource},
        {"Chassis", chassisTestFiles, "", &createChassisResource},
    }),
    [](const testing::TestParamInfo<BejEncoderTest::ParamType>& info) {
    return info.param.testName;
});

} // namespace libbej
