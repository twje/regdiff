// Project
#include "Regdiff/RegistryComparator.h"

// Tests
#include "TestSupport.h"

// Third party
#include <gtest/gtest.h>

// Standard
#include <string>
#include <vector>

namespace
{
  constexpr std::string_view kExampleKey = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Example";

  ComparisonResult Compare(const RegistrySnapshot& before, const RegistrySnapshot& after)
  {
    return RegistryComparator().Compare(before, after);
  }

  // One Registry Key holding one Registry Value, which is all most of these
  // tests need.
  RegistrySnapshot ExampleWith(RegistryValue value)
  {
    return test::MakeSnapshot({ { .path = std::string(kExampleKey), .values = { std::move(value) } } });
  }
}

TEST(RegistryComparatorTests, ReportsNoDifferencesForIdenticalSnapshots)
{
  const RegistrySnapshot snapshot = ExampleWith(test::MakeStringValue("Name", "Example"));

  const ComparisonResult result = Compare(snapshot, snapshot);

  EXPECT_FALSE(result.HasDifferences());
  EXPECT_TRUE(result.added_keys.empty());
  EXPECT_TRUE(result.removed_keys.empty());
  EXPECT_TRUE(result.value_differences.empty());
}

TEST(RegistryComparatorTests, ReportsNoDifferencesForTwoEmptySnapshots)
{
  EXPECT_FALSE(Compare(test::MakeSnapshot({}), test::MakeSnapshot({})).HasDifferences());
}

TEST(RegistryComparatorTests, DetectsAnAddedRegistryKey)
{
  const ComparisonResult result =
    Compare(test::MakeSnapshot({ { .path = std::string(kExampleKey) } }),
            test::MakeSnapshot({ { .path = std::string(kExampleKey) },
                                 { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\NewApplication" } }));

  ASSERT_EQ(result.added_keys.size(), 1u);
  EXPECT_EQ(result.added_keys.front(), "HKEY_LOCAL_MACHINE\\SOFTWARE\\NewApplication");
  EXPECT_TRUE(result.removed_keys.empty());
  EXPECT_TRUE(result.value_differences.empty());
}

TEST(RegistryComparatorTests, DetectsARemovedRegistryKey)
{
  const ComparisonResult result =
    Compare(test::MakeSnapshot({ { .path = std::string(kExampleKey) },
                                 { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\OldApplication" } }),
            test::MakeSnapshot({ { .path = std::string(kExampleKey) } }));

  ASSERT_EQ(result.removed_keys.size(), 1u);
  EXPECT_EQ(result.removed_keys.front(), "HKEY_LOCAL_MACHINE\\SOFTWARE\\OldApplication");
  EXPECT_TRUE(result.added_keys.empty());
}

TEST(RegistryComparatorTests, AddedRegistryKeysDoNotReportTheirRegistryValues)
{
  const ComparisonResult result = Compare(test::MakeSnapshot({}), ExampleWith(test::MakeStringValue("Name", "x")));

  ASSERT_EQ(result.added_keys.size(), 1u);
  EXPECT_TRUE(result.value_differences.empty());
}

TEST(RegistryComparatorTests, DetectsAnAddedRegistryValue)
{
  const ComparisonResult result = Compare(test::MakeSnapshot({ { .path = std::string(kExampleKey) } }),
                                          ExampleWith(test::MakeValue("Enabled", 4, { 0x01, 0, 0, 0 })));

  ASSERT_EQ(result.value_differences.size(), 1u);

  const ValueDifference& difference = result.value_differences.front();
  EXPECT_EQ(difference.type, DifferenceType::kValueAdded);
  EXPECT_EQ(difference.key_path, kExampleKey);
  EXPECT_EQ(difference.value_name, "Enabled");
  EXPECT_FALSE(difference.before.has_value());
  ASSERT_TRUE(difference.after.has_value());
  EXPECT_EQ(difference.after->type, 4u);
}

TEST(RegistryComparatorTests, DetectsARemovedRegistryValue)
{
  const ComparisonResult result = Compare(ExampleWith(test::MakeValue("Enabled", 4, { 0x01, 0, 0, 0 })),
                                          test::MakeSnapshot({ { .path = std::string(kExampleKey) } }));

  ASSERT_EQ(result.value_differences.size(), 1u);

  const ValueDifference& difference = result.value_differences.front();
  EXPECT_EQ(difference.type, DifferenceType::kValueRemoved);
  EXPECT_EQ(difference.value_name, "Enabled");
  ASSERT_TRUE(difference.before.has_value());
  EXPECT_EQ(difference.before->data, (std::vector<std::uint8_t>{ 0x01, 0, 0, 0 }));
  EXPECT_FALSE(difference.after.has_value());
}

TEST(RegistryComparatorTests, DetectsModifiedRegistryValueData)
{
  const ComparisonResult result = Compare(ExampleWith(test::MakeStringValue("Version", "1.0")),
                                          ExampleWith(test::MakeStringValue("Version", "2.0")));

  ASSERT_EQ(result.value_differences.size(), 1u);

  const ValueDifference& difference = result.value_differences.front();
  EXPECT_EQ(difference.type, DifferenceType::kValueModified);
  ASSERT_TRUE(difference.before.has_value());
  ASSERT_TRUE(difference.after.has_value());
  EXPECT_NE(difference.before->data, difference.after->data);
}

TEST(RegistryComparatorTests, DetectsAChangeOfRegistryValueTypeAlone)
{
  const ComparisonResult result = Compare(ExampleWith(test::MakeValue("Blob", 3, { 0x00 })),
                                          ExampleWith(test::MakeValue("Blob", 7, { 0x00 })));

  ASSERT_EQ(result.value_differences.size(), 1u);
  EXPECT_EQ(result.value_differences.front().type, DifferenceType::kValueModified);
  EXPECT_EQ(result.value_differences.front().before->type, 3u);
  EXPECT_EQ(result.value_differences.front().after->type, 7u);
}

TEST(RegistryComparatorTests, DetectsAModifiedDefaultRegistryValue)
{
  const ComparisonResult result =
    Compare(ExampleWith(test::MakeStringValue("", "before")), ExampleWith(test::MakeStringValue("", "after")));

  ASSERT_EQ(result.value_differences.size(), 1u);
  EXPECT_EQ(result.value_differences.front().type, DifferenceType::kValueModified);
  EXPECT_TRUE(result.value_differences.front().value_name.empty());
}

TEST(RegistryComparatorTests, ComparesRegistryValuesWithUnrecognisedValueTypes)
{
  const RegistrySnapshot before = ExampleWith(test::MakeValue("Level", 0x200000, { 0x01, 0x02 }));

  EXPECT_FALSE(Compare(before, before).HasDifferences());

  const ComparisonResult changed =
    Compare(before, ExampleWith(test::MakeValue("Level", 0x200000, { 0x01, 0x03 })));

  ASSERT_EQ(changed.value_differences.size(), 1u);
  EXPECT_EQ(changed.value_differences.front().type, DifferenceType::kValueModified);
}

TEST(RegistryComparatorTests, RegistryKeyPathsAreCaseInsensitive)
{
  const ComparisonResult result =
    Compare(test::MakeSnapshot({ { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Example" } }),
            test::MakeSnapshot({ { .path = "hkey_local_machine\\software\\example" } }));

  EXPECT_FALSE(result.HasDifferences());
}

TEST(RegistryComparatorTests, RegistryValueNamesAreCaseInsensitive)
{
  const ComparisonResult result =
    Compare(ExampleWith(test::MakeStringValue("Name", "Example")),
            ExampleWith(test::MakeStringValue("NAME", "Example")));

  EXPECT_FALSE(result.HasDifferences());
}

TEST(RegistryComparatorTests, RegistryValueDataIsCaseSensitive)
{
  const ComparisonResult result =
    Compare(ExampleWith(test::MakeStringValue("Name", "Example")),
            ExampleWith(test::MakeStringValue("Name", "EXAMPLE")));

  ASSERT_EQ(result.value_differences.size(), 1u);
  EXPECT_EQ(result.value_differences.front().type, DifferenceType::kValueModified);
}

TEST(RegistryComparatorTests, ReportsDifferencesInADeterministicOrder)
{
  const ComparisonResult result = Compare(
    test::MakeSnapshot({ { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Beta" },
                         { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Alpha",
                           .values = { test::MakeStringValue("Second", "2"),
                                       test::MakeStringValue("First", "1") } } }),
    test::MakeSnapshot({ { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Alpha" } }));

  ASSERT_EQ(result.removed_keys.size(), 1u);
  EXPECT_EQ(result.removed_keys.front(), "HKEY_LOCAL_MACHINE\\SOFTWARE\\Beta");

  ASSERT_EQ(result.value_differences.size(), 2u);
  EXPECT_EQ(result.value_differences[0].value_name, "First");
  EXPECT_EQ(result.value_differences[1].value_name, "Second");
}

TEST(RegistryComparatorTests, ComparesRegistryKeysThatOnlyAppearOnOneSide)
{
  // Every Registry Key on one side sorts after every Registry Key on the other,
  // so the walk has to drain each list in turn.
  const ComparisonResult result =
    Compare(test::MakeSnapshot({ { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Alpha" },
                                 { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bravo" } }),
            test::MakeSnapshot({ { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Yankee" },
                                 { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Zulu" } }));

  EXPECT_EQ(result.removed_keys,
            (std::vector<std::string>{ "HKEY_LOCAL_MACHINE\\SOFTWARE\\Alpha",
                                       "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bravo" }));
  EXPECT_EQ(result.added_keys,
            (std::vector<std::string>{ "HKEY_LOCAL_MACHINE\\SOFTWARE\\Yankee",
                                       "HKEY_LOCAL_MACHINE\\SOFTWARE\\Zulu" }));
}

TEST(RegistryComparatorTests, ComparesSeveralRegistryValuesInOneRegistryKey)
{
  const ComparisonResult result = Compare(
    test::MakeSnapshot({ { .path = std::string(kExampleKey),
                           .values = { test::MakeStringValue("Kept", "same"),
                                       test::MakeStringValue("Gone", "x"),
                                       test::MakeStringValue("Changed", "before") } } }),
    test::MakeSnapshot({ { .path = std::string(kExampleKey),
                           .values = { test::MakeStringValue("Kept", "same"),
                                       test::MakeStringValue("Changed", "after"),
                                       test::MakeStringValue("New", "y") } } }));

  ASSERT_EQ(result.value_differences.size(), 3u);
  EXPECT_EQ(result.value_differences[0].value_name, "Changed");
  EXPECT_EQ(result.value_differences[0].type, DifferenceType::kValueModified);
  EXPECT_EQ(result.value_differences[1].value_name, "Gone");
  EXPECT_EQ(result.value_differences[1].type, DifferenceType::kValueRemoved);
  EXPECT_EQ(result.value_differences[2].value_name, "New");
  EXPECT_EQ(result.value_differences[2].type, DifferenceType::kValueAdded);
}
