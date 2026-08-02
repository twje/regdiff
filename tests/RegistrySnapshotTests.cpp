// Project
#include "Regdiff/RegistrySnapshot.h"

// Tests
#include "TestSupport.h"

// Third party
#include <gtest/gtest.h>

// Standard
#include <string>
#include <vector>

TEST(RegistrySnapshotTests, ExpandsAnAbbreviatedRegistryRoot)
{
  EXPECT_EQ(ExpandRegistryRoot("HKCR"), "HKEY_CLASSES_ROOT");
  EXPECT_EQ(ExpandRegistryRoot("HKCU"), "HKEY_CURRENT_USER");
  EXPECT_EQ(ExpandRegistryRoot("HKLM"), "HKEY_LOCAL_MACHINE");
  EXPECT_EQ(ExpandRegistryRoot("HKU"), "HKEY_USERS");
  EXPECT_EQ(ExpandRegistryRoot("HKCC"), "HKEY_CURRENT_CONFIG");
}

TEST(RegistrySnapshotTests, ExpandsAnAbbreviatedRegistryRootWithAPathBelowIt)
{
  EXPECT_EQ(ExpandRegistryRoot("HKLM\\SOFTWARE"), "HKEY_LOCAL_MACHINE\\SOFTWARE");
  EXPECT_EQ(ExpandRegistryRoot("HKCU\\Software\\Example"), "HKEY_CURRENT_USER\\Software\\Example");
}

TEST(RegistrySnapshotTests, KeepsTheCaseOfEverythingBelowTheRegistryRoot)
{
  EXPECT_EQ(ExpandRegistryRoot("HKLM\\SoFtWaRe\\ExAmPlE"), "HKEY_LOCAL_MACHINE\\SoFtWaRe\\ExAmPlE");
}

TEST(RegistrySnapshotTests, ExpandsARegistryRootWhateverItsCase)
{
  EXPECT_EQ(ExpandRegistryRoot("hklm\\Software"), "HKEY_LOCAL_MACHINE\\Software");
  EXPECT_EQ(ExpandRegistryRoot("hKeY_lOcAl_MaChInE\\Software"), "HKEY_LOCAL_MACHINE\\Software");
}

TEST(RegistrySnapshotTests, LeavesAFullRegistryRootAlone)
{
  EXPECT_EQ(ExpandRegistryRoot("HKEY_LOCAL_MACHINE\\SOFTWARE"), "HKEY_LOCAL_MACHINE\\SOFTWARE");
  EXPECT_EQ(ExpandRegistryRoot("HKEY_USERS"), "HKEY_USERS");
}

TEST(RegistrySnapshotTests, LeavesAPathWithNoRecognisedRegistryRootAlone)
{
  // Reported later, by whatever fails to read it.
  EXPECT_EQ(ExpandRegistryRoot("NOT_A_HIVE\\Example"), "NOT_A_HIVE\\Example");
  EXPECT_EQ(ExpandRegistryRoot(""), "");

  // Only a whole Registry root is expanded, never part of one.
  EXPECT_EQ(ExpandRegistryRoot("HKLMX\\Example"), "HKLMX\\Example");
}

TEST(RegistrySnapshotTests, ListsTheStandardRegistryHives)
{
  EXPECT_EQ(StandardRegistryRoots(),
            (std::vector<std::string>{ "HKEY_CLASSES_ROOT",
                                       "HKEY_CURRENT_USER",
                                       "HKEY_LOCAL_MACHINE",
                                       "HKEY_USERS",
                                       "HKEY_CURRENT_CONFIG" }));
}

TEST(RegistrySnapshotTests, EveryStandardRegistryRootIsAlreadyExpanded)
{
  for (const std::string& root : StandardRegistryRoots())
  {
    EXPECT_EQ(ExpandRegistryRoot(root), root);
  }
}

TEST(RegistrySnapshotTests, ComparesRegistryNamesWithoutRegardToCase)
{
  EXPECT_EQ(CompareRegistryNames("Example", "EXAMPLE"), 0);
  EXPECT_LT(CompareRegistryNames("alpha", "Beta"), 0);
  EXPECT_GT(CompareRegistryNames("Beta", "alpha"), 0);
  EXPECT_LT(CompareRegistryNames("Same", "Sameness"), 0);
}

TEST(RegistrySnapshotTests, CanonicaliseDropsARepeatedRegistryKey)
{
  RegistrySnapshot snapshot;
  snapshot.keys = { { .path = "HKEY_CURRENT_USER\\Zulu" },
                    { .path = "HKEY_CURRENT_USER\\alpha" },
                    { .path = "hkey_current_user\\ZULU" } };

  Canonicalise(snapshot);

  ASSERT_EQ(snapshot.keys.size(), 2u);
  EXPECT_EQ(snapshot.keys[0].path, "HKEY_CURRENT_USER\\alpha");
  EXPECT_EQ(snapshot.keys[1].path, "HKEY_CURRENT_USER\\Zulu");
}

TEST(RegistrySnapshotTests, FormatsBytesAsLowercaseHexadecimal)
{
  EXPECT_EQ(ToHexadecimal(std::vector<std::uint8_t>{}), "");
  EXPECT_EQ(ToHexadecimal(std::vector<std::uint8_t>{ 0x00, 0x0F, 0xAB, 0xFF }), "000fabff");
}
