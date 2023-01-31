# libbej

This library can be used to decode RDE bej data. More details on how to use
the library will be added in the future.

# BEJ encoding

## Storing a JSON object for BEJ encoding

One of the inputs to the BEJ encoding is a JSON object. We could provide a JSON object in string format but processing a string during BEJ encoding can be too much for devices with less computational power (Eg: micro-controller). And also the devices will have to update the fields in the JSON before the encoding. Eg: New sensor reading. Its not convenient to update a JSON in string format.

In order to describe a JSON object in a BEJ encoding friendly format, we can use the API provided in bej_tree.h

### BEJ JSON tree API

bej_tree.h provides an API to maintain a JSON tree. API provides different node types derived based on BEJ data types. The user of the API needs to allocate the memory for each node. The API simply maintains the Parent child relationship through linked lists and provide an interface to set values for different types of properties.

### Design goals

* Lightweight implementation.
  - This needs to support devices with less computational power like micro-controllers. And we also need to integrate this with the BEJ encoder. Therefore implemented in C.


* No memory allocations.
  * Different platforms will use its own way of handling memory. So the memory allocation for the nodes should be done by the user. This also allows to keep the library simple and free of memory bugs.

* Being able to dynamically add new data to the JSON tree.
  * User should be able to add things to he tree during runtime. Eg: discovering sensors during runtime.

* Being able to easily perform BEJ encoding on the resource.
   * API structures defines additional fields that will be used during BEJ encoding. This way BEJ encoder doesn't need to allocate this memory during during encoding process.

### Usage

Consider the following example.

```
{
    "@odata.id": "/redfish/v1/Chassis",
    "@odata.type": "#ChassisCollection.ChassisCollection",
    "Members": [
        {
            "@odata.id": "/redfish/v1/Chassis/SomeChassis"
        }
    ],
    "Members@odata.count": 1,
}
```

In order to represent the above JSON object, first the user needs storage. So lets define the following structure to store the nodes.

```
struct redfish_collection_member
{
    // Nameless Set type of the "Members" array property.
    struct RedfishPropertyParent nameless_set;
    // odata.id of the member.
    struct RedfishPropertyLeafString odata_id;
};

struct redfish_collection_resource
{
    // Stores the root Set type object.
    struct RedfishPropertyParent root;
    struct RedfishPropertyLeafString odata_id;
    struct RedfishPropertyLeafString odata_type;
    // Storage for the "Members" array property.
    struct RedfishPropertyParent members_array;
    // Array to store all the possible members in the members array.
    struct redfish_collection_member members[MAXIMUM_POSSIBLE_MEMBERS];
    // Annotated "Members: property.
    struct RedfishPropertyParent annotated_members;
    // Storage for annotated @odata.count property.
    struct RedfishPropertyLeafInt members_count;
};
```

Then the user can use the above struct and populate it using the bej_tree API.

```
struct redfish_collection_resource col;

// All parent nodes (Set, Array, Annotated) should be initialized before using.
bejTreeInitSet(&col.root, NULL);

// Add the @odata.id and @odata.type to the root node (which is a nameless Set).
bejTreeAddString(&col.root, &col.odata_id, "@odata.id", "/redfish/v1/Chassis");
bejTreeAddString(&col.root, &col.odata_type, "@odata.type", "#ChassisCollection.ChassisCollection");

// Initialize the "Members" array property 
bejTreeInitArray(&col.members_array, "Members");
// Link the array property to the root Set property
bejTreeLinkChildToParent(&col.root, &col.members_array);

// Initialize a member entry
struct redfish_collection_member* member = &col.members[0];
bejTreeInitSet(&member->nameless_set, NULL);
bejTreeAddString(&member->nameless_set, &member->odata_id, "@odata.id", "/redfish/v1/Chassis/SomeChassis");

// Add the member to the "Members" array
bejTreeLinkChildToParent(&col.members_array, &member->nameless_set);

// Initialize the annotated property
bejTreeInitPropertyAnnotated(&col.annotated_members, "Members");
bejTreeAddInteger(&col.annotated_members, &col.members_count, "@odata.count", 1);
bejTreeLinkChildToParent(&col.root, &col.annotated_members);
```

Then we can simply pass a pointer to the `col.root` to the BEJ encoder.

### Future improvements

* Check for duplicates when inserting.

### ALternatives considered.

* A list of some existing JSON libraries: https://www.json.org/json-en.html
* Benchmarks for these libraries: https://github.com/miloyip/nativejson-benchmark

One of the main purposes of representing data this way is to support BEJ encoding. Therefore it is better to create a lightweight JSON api suited for this specific purpose rather than trying to reuse an existing one.