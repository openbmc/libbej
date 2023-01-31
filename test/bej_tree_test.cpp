#include "bej_tree.h"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libbej
{

TEST(BejTreeTest, InitSet)
{
    const char* name = "SomeProperty";
    struct RedfishPropertyParent node;
    bejTreeInitSet(&node, name);

    EXPECT_THAT(node.nodeAttr.name, name);
    EXPECT_THAT(node.nodeAttr.format.principalDataType, bejSet);
    EXPECT_THAT(node.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(node.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(node.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(node.nodeAttr.sibling, nullptr);
    EXPECT_THAT(node.nChildren, 0);
    EXPECT_THAT(node.firstChild, nullptr);
    EXPECT_THAT(node.lastChild, nullptr);
}

TEST(BejTreeTest, InitArray)
{
    const char* name = "SomeProperty";
    struct RedfishPropertyParent node;
    bejTreeInitArray(&node, name);

    EXPECT_THAT(node.nodeAttr.name, name);
    EXPECT_THAT(node.nodeAttr.format.principalDataType, bejArray);
    EXPECT_THAT(node.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(node.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(node.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(node.nodeAttr.sibling, nullptr);
    EXPECT_THAT(node.nChildren, 0);
    EXPECT_THAT(node.firstChild, nullptr);
    EXPECT_THAT(node.lastChild, nullptr);
}

TEST(BejTreeTest, InitAnnotatedProp)
{
    const char* name = "SomeProperty";
    struct RedfishPropertyParent node;
    bejTreeInitPropertyAnnotated(&node, name);

    EXPECT_THAT(node.nodeAttr.name, name);
    EXPECT_THAT(node.nodeAttr.format.principalDataType, bejPropertyAnnotation);
    EXPECT_THAT(node.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(node.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(node.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(node.nodeAttr.sibling, nullptr);
    EXPECT_THAT(node.nChildren, 0);
    EXPECT_THAT(node.firstChild, nullptr);
    EXPECT_THAT(node.lastChild, nullptr);
}

TEST(BejTreeTest, ChildLinking)
{
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafInt child1;
    struct RedfishPropertyLeafInt child2;

    bejTreeInitSet(&parent, nullptr);
    EXPECT_THAT(parent.nChildren, 0);
    EXPECT_THAT(parent.firstChild, nullptr);
    EXPECT_THAT(parent.lastChild, nullptr);

    bejTreeAddInteger(&parent, &child1, nullptr, 1024);
    EXPECT_THAT(parent.nChildren, 1);
    EXPECT_THAT(parent.firstChild, &child1);
    EXPECT_THAT(parent.lastChild, &child1);

    bejTreeAddInteger(&parent, &child2, nullptr, 20);
    EXPECT_THAT(parent.nChildren, 2);
    EXPECT_THAT(parent.firstChild, &child1);
    EXPECT_THAT(parent.lastChild, &child2);

    // child2 should be a sibling of child1.
    EXPECT_THAT(child1.leaf.nodeAttr.sibling, &child2);
}

TEST(BejTreeTest, AddInteger)
{
    const char* name = "SomeProperty";
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafInt child;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddInteger(&parent, &child, name, 1024);

    EXPECT_THAT(child.leaf.nodeAttr.name, name);
    EXPECT_THAT(child.leaf.nodeAttr.format.principalDataType, bejInteger);
    EXPECT_THAT(child.leaf.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.sibling, nullptr);
    EXPECT_THAT(child.value, 1024);
}

TEST(BejTreeTest, SetInteger)
{
    const char* name = "SomeProperty";
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafInt child;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddInteger(&parent, &child, name, 1024);

    EXPECT_THAT(child.value, 1024);
    bejTreeSetInteger(&child, 20);
    EXPECT_THAT(child.value, 20);
}

TEST(BejTreeTest, AddEnum)
{
    const char* name = "SomeProperty";
    const char* enumValue = "EnumValue";
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafEnum child;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddEnum(&parent, &child, name, enumValue);

    EXPECT_THAT(child.leaf.nodeAttr.name, name);
    EXPECT_THAT(child.leaf.nodeAttr.format.principalDataType, bejEnum);
    EXPECT_THAT(child.leaf.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.sibling, nullptr);
    EXPECT_THAT(child.value, enumValue);
}

TEST(BejTreeTest, AddString)
{
    const char* name = "SomeProperty";
    const char* stringValue = "StringValue";
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafString child;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddString(&parent, &child, name, stringValue);

    EXPECT_THAT(child.leaf.nodeAttr.name, name);
    EXPECT_THAT(child.leaf.nodeAttr.format.principalDataType, bejString);
    EXPECT_THAT(child.leaf.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.sibling, nullptr);
    EXPECT_THAT(child.value, stringValue);
}

TEST(BejTreeTest, AddReal)
{
    const char* name = "SomeProperty";
    double value = 10.50;
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafReal child;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddReal(&parent, &child, name, value);

    EXPECT_THAT(child.leaf.nodeAttr.name, name);
    EXPECT_THAT(child.leaf.nodeAttr.format.principalDataType, bejReal);
    EXPECT_THAT(child.leaf.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.sibling, nullptr);
    EXPECT_THAT(child.value, value);
}

TEST(BejTreeTest, AddBool)
{
    const char* name = "SomeProperty";
    bool value = true;
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafBool child;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddBool(&parent, &child, name, value);

    EXPECT_THAT(child.leaf.nodeAttr.name, name);
    EXPECT_THAT(child.leaf.nodeAttr.format.principalDataType, bejBoolean);
    EXPECT_THAT(child.leaf.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.format.nullableProperty, 0);
    EXPECT_THAT(child.leaf.nodeAttr.sibling, nullptr);
    EXPECT_THAT(child.value, value);
}

TEST(BejTreeTest, NodeFlags)
{
    struct RedfishPropertyParent parent;
    bejTreeInitSet(&parent, nullptr);
    EXPECT_THAT(parent.nodeAttr.format.deferredBinding, 0);
    EXPECT_THAT(parent.nodeAttr.format.readOnlyProperty, 0);
    EXPECT_THAT(parent.nodeAttr.format.nullableProperty, 0);

    bejTreeUpdateNodeFlags(&parent.nodeAttr, true, true, true);
    EXPECT_THAT(parent.nodeAttr.format.deferredBinding, 1);
    EXPECT_THAT(parent.nodeAttr.format.readOnlyProperty, 1);
    EXPECT_THAT(parent.nodeAttr.format.nullableProperty, 1);
}

TEST(BejTreeTest, NodeType)
{
    struct RedfishPropertyParent parent;
    struct RedfishPropertyLeafBool child1;
    struct RedfishPropertyLeafReal child2;

    bejTreeInitSet(&parent, nullptr);
    bejTreeAddBool(&parent, &child1, nullptr, true);
    bejTreeAddReal(&parent, &child2, nullptr, 10.5);

    EXPECT_THAT(bejTreeIsParentType(&parent.nodeAttr), true);
    EXPECT_THAT(bejTreeIsParentType(&child1.leaf.nodeAttr), false);
    EXPECT_THAT(bejTreeIsParentType(&child2.leaf.nodeAttr), false);
}

} // namespace libbej
