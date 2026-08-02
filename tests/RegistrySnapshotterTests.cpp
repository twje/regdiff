// Project
#include "Regdiff/RegistrySnapshotter.h"

// Tests
#include "TestSupport.h"

// Third party
#include <gtest/gtest.h>

// Standard
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr std::string_view kRoot = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Example";

  RegistrySnapshot Capture(const test::FakeRegistry& registry, std::vector<std::string> roots)
  {
    return RegistrySnapshotter(registry).Capture(roots);
  }

  std::vector<std::string> Paths(const RegistrySnapshot& snapshot)
  {
    std::vector<std::string> paths;

    for (const RegistryKey& key : snapshot.keys)
    {
      paths.push_back(key.path);
    }

    return paths;
  }

  const RegistryKey* FindKey(const RegistrySnapshot& snapshot, std::string_view path)
  {
    for (const RegistryKey& key : snapshot.keys)
    {
      if (key.path == path)
      {
        return &key;
      }
    }

    ADD_FAILURE() << "missing Registry Key: " << path;
    return nullptr;
  }
}

TEST(RegistrySnapshotterTests, CapturesNothingFromAnEmptyRegistry)
{
  const test::FakeRegistry registry;

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  EXPECT_TRUE(snapshot.keys.empty());
  ASSERT_EQ(snapshot.diagnostics.size(), 1u);
  EXPECT_NE(snapshot.diagnostics.front().find("does not exist"), std::string::npos);
}

TEST(RegistrySnapshotterTests, CapturesASingleRegistryKey)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot), { test::MakeStringValue("Name", "Example") });

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  ASSERT_EQ(snapshot.keys.size(), 1u);
  EXPECT_EQ(snapshot.keys.front().path, kRoot);
  ASSERT_EQ(snapshot.keys.front().values.size(), 1u);
  EXPECT_EQ(snapshot.keys.front().values.front().name, "Name");
  EXPECT_TRUE(snapshot.diagnostics.empty());
}

TEST(RegistrySnapshotterTests, RecordsTheRootsItWasAskedFor)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot));

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  ASSERT_EQ(snapshot.roots.size(), 1u);
  EXPECT_EQ(snapshot.roots.front(), kRoot);
  EXPECT_EQ(snapshot.schema_version, kSnapshotSchemaVersion);
}

TEST(RegistrySnapshotterTests, CapturesNestedRegistryKeys)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot))
    .AddKey(std::string(kRoot) + "\\Child")
    .AddKey(std::string(kRoot) + "\\Child\\Grandchild")
    .AddKey(std::string(kRoot) + "\\Sibling");

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  EXPECT_EQ(Paths(snapshot),
            (std::vector<std::string>{ std::string(kRoot),
                                       std::string(kRoot) + "\\Child",
                                       std::string(kRoot) + "\\Child\\Grandchild",
                                       std::string(kRoot) + "\\Sibling" }));
}

TEST(RegistrySnapshotterTests, CapturesTheDefaultRegistryValue)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot), { test::MakeStringValue("", "Default data") });

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  const RegistryKey* key = FindKey(snapshot, kRoot);
  ASSERT_NE(key, nullptr);
  ASSERT_EQ(key->values.size(), 1u);
  EXPECT_TRUE(key->values.front().name.empty());
}

TEST(RegistrySnapshotterTests, CapturesEveryRegistryValueTypeUnchanged)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot),
                  { test::MakeValue("Binary", 3, { 0x01, 0x02, 0xFF }),
                    test::MakeValue("Dword", 4, { 0x01, 0x00, 0x00, 0x00 }),
                    test::MakeValue("Empty", 0, {}),
                    test::MakeValue("Unknown", 0x200000, { 0xAB }) });

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  const RegistryKey* key = FindKey(snapshot, kRoot);
  ASSERT_NE(key, nullptr);
  ASSERT_EQ(key->values.size(), 4u);

  EXPECT_EQ(key->values[0].type, 3u);
  EXPECT_EQ(key->values[0].data, (std::vector<std::uint8_t>{ 0x01, 0x02, 0xFF }));
  EXPECT_EQ(key->values[1].type, 4u);
  EXPECT_EQ(key->values[2].type, 0u);
  EXPECT_TRUE(key->values[2].data.empty());

  // A Registry Value Type RegDiff does not recognise is kept as it was found.
  EXPECT_EQ(key->values[3].type, 0x200000u);
  EXPECT_EQ(key->values[3].data, (std::vector<std::uint8_t>{ 0xAB }));
}

TEST(RegistrySnapshotterTests, RecordsAnInaccessibleRegistryKeyAndCarriesOn)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot))
    .DenyKey(std::string(kRoot) + "\\Locked")
    .AddKey(std::string(kRoot) + "\\Readable");

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  EXPECT_EQ(Paths(snapshot),
            (std::vector<std::string>{ std::string(kRoot), std::string(kRoot) + "\\Readable" }));

  ASSERT_EQ(snapshot.diagnostics.size(), 1u);
  EXPECT_NE(snapshot.diagnostics.front().find("\\Locked"), std::string::npos);
  EXPECT_NE(snapshot.diagnostics.front().find("access is denied"), std::string::npos);
}

TEST(RegistrySnapshotterTests, DoesNotDescendIntoAnInaccessibleRegistryKey)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot))
    .DenyKey(std::string(kRoot) + "\\Locked")
    .AddKey(std::string(kRoot) + "\\Locked\\Hidden");

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  EXPECT_EQ(Paths(snapshot), (std::vector<std::string>{ std::string(kRoot) }));
}

TEST(RegistrySnapshotterTests, CapturesSeveralRoots)
{
  test::FakeRegistry registry;
  registry.AddKey("HKEY_CURRENT_USER\\Software").AddKey("HKEY_LOCAL_MACHINE\\SOFTWARE");

  const RegistrySnapshot snapshot =
    Capture(registry, { "HKEY_LOCAL_MACHINE\\SOFTWARE", "HKEY_CURRENT_USER\\Software" });

  EXPECT_EQ(Paths(snapshot),
            (std::vector<std::string>{ "HKEY_CURRENT_USER\\Software", "HKEY_LOCAL_MACHINE\\SOFTWARE" }));
}

TEST(RegistrySnapshotterTests, ListsARegistryKeyReachedByTwoOverlappingRootsOnlyOnce)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot)).AddKey(std::string(kRoot) + "\\Child");

  const RegistrySnapshot snapshot =
    Capture(registry, { std::string(kRoot), std::string(kRoot) + "\\Child" });

  EXPECT_EQ(Paths(snapshot),
            (std::vector<std::string>{ std::string(kRoot), std::string(kRoot) + "\\Child" }));
}

TEST(RegistrySnapshotterTests, SortsRegistryKeysAndRegistryValues)
{
  test::FakeRegistry registry;
  registry.AddKey(std::string(kRoot))
    .AddKey(std::string(kRoot) + "\\Zulu")
    .AddKey(std::string(kRoot) + "\\alpha",
            { test::MakeStringValue("Second", "2"), test::MakeStringValue("First", "1") });

  const RegistrySnapshot snapshot = Capture(registry, { std::string(kRoot) });

  // Sorted case-insensitively, so "alpha" comes before "Zulu".
  EXPECT_EQ(Paths(snapshot),
            (std::vector<std::string>{ std::string(kRoot),
                                       std::string(kRoot) + "\\alpha",
                                       std::string(kRoot) + "\\Zulu" }));

  const RegistryKey* key = FindKey(snapshot, std::string(kRoot) + "\\alpha");
  ASSERT_NE(key, nullptr);
  ASSERT_EQ(key->values.size(), 2u);
  EXPECT_EQ(key->values[0].name, "First");
  EXPECT_EQ(key->values[1].name, "Second");
}
