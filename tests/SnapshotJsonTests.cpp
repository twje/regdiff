// Project
#include "Regdiff/RegistryError.h"
#include "Regdiff/SnapshotJson.h"

// Tests
#include "TestSupport.h"

// Third party
#include <gtest/gtest.h>

// Standard
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  std::string Write(const RegistrySnapshot& snapshot)
  {
    std::ostringstream stream;
    SnapshotWriter().Write(stream, snapshot);

    return stream.str();
  }

  RegistrySnapshot Read(std::string_view json)
  {
    std::istringstream stream{ std::string(json) };
    return SnapshotReader().Read(stream);
  }

  std::string ErrorMessage(std::string_view json)
  {
    try
    {
      Read(json);
    }
    catch (const RegistryError& error)
    {
      return error.what();
    }

    return "no error was reported";
  }

  // A valid snapshot with one Registry Key, used as the starting point for the
  // tests that damage one field at a time.
  constexpr std::string_view kValidSnapshot = R"({
    "schemaVersion": 1,
    "capturedAt": "2026-08-02T09:15:00Z",
    "computerName": "TESTMACHINE",
    "roots": ["HKEY_CURRENT_USER\\Software"],
    "diagnostics": [],
    "keys": [
      {
        "path": "HKEY_CURRENT_USER\\Software\\Example",
        "values": [ { "name": "Name", "type": 1, "data": "4500" } ]
      }
    ]
  })";

  RegistrySnapshot ExampleSnapshot()
  {
    RegistrySnapshot snapshot = test::MakeSnapshot(
      { { .path = "HKEY_CURRENT_USER\\Software\\Example",
          .values = { test::MakeValue("Binary", 3, { 0x00, 0x01, 0xFE, 0xFF }),
                      test::MakeValue("", 1, { 0x41, 0x00, 0x00, 0x00 }),
                      test::MakeValue("Empty", 0, {}),
                      test::MakeValue("Unknown", 0x200000, { 0xAB, 0xCD }) } },
        { .path = "HKEY_CURRENT_USER\\Software\\Example\\Child" } });

    snapshot.roots = { "HKEY_CURRENT_USER\\Software" };
    snapshot.diagnostics = { "HKEY_CURRENT_USER\\Software\\Locked: access is denied" };

    return snapshot;
  }
}

TEST(SnapshotJsonTests, WritesEveryDocumentedField)
{
  const std::string json = Write(ExampleSnapshot());

  EXPECT_NE(json.find("\"schemaVersion\":1"), std::string::npos) << json;
  EXPECT_NE(json.find("\"capturedAt\":\"2026-08-02T09:15:00Z\""), std::string::npos) << json;
  EXPECT_NE(json.find("\"computerName\":\"TESTMACHINE\""), std::string::npos) << json;
  EXPECT_NE(json.find("\"roots\":"), std::string::npos) << json;
  EXPECT_NE(json.find("\"diagnostics\":"), std::string::npos) << json;
  EXPECT_NE(json.find("\"keys\":"), std::string::npos) << json;
}

TEST(SnapshotJsonTests, WritesRegistryValueDataAsLowercaseHexadecimal)
{
  const std::string json = Write(ExampleSnapshot());

  EXPECT_NE(json.find("\"data\":\"0001feff\""), std::string::npos) << json;
  EXPECT_NE(json.find("\"data\":\"abcd\""), std::string::npos) << json;
  EXPECT_NE(json.find("\"data\":\"\""), std::string::npos) << json;
}

TEST(SnapshotJsonTests, RoundTripsASnapshot)
{
  const RegistrySnapshot original = ExampleSnapshot();
  const RegistrySnapshot restored = Read(Write(original));

  EXPECT_EQ(restored.schema_version, original.schema_version);
  EXPECT_EQ(restored.captured_at, original.captured_at);
  EXPECT_EQ(restored.computer_name, original.computer_name);
  EXPECT_EQ(restored.roots, original.roots);
  EXPECT_EQ(restored.diagnostics, original.diagnostics);

  ASSERT_EQ(restored.keys.size(), original.keys.size());

  for (std::size_t index = 0; index < original.keys.size(); ++index)
  {
    const RegistryKey& was = original.keys[index];
    const RegistryKey& now = restored.keys[index];

    EXPECT_EQ(now.path, was.path);
    ASSERT_EQ(now.values.size(), was.values.size());

    for (std::size_t value = 0; value < was.values.size(); ++value)
    {
      EXPECT_EQ(now.values[value].name, was.values[value].name);
      EXPECT_EQ(now.values[value].type, was.values[value].type);
      EXPECT_EQ(now.values[value].data, was.values[value].data);
    }
  }
}

TEST(SnapshotJsonTests, RoundTripsAnUnrecognisedRegistryValueType)
{
  const RegistrySnapshot restored = Read(Write(test::MakeSnapshot(
    { { .path = "HKEY_CURRENT_USER\\Software\\Example",
        .values = { test::MakeValue("Odd", 0xFFFFFFFF, { 0x01 }) } } })));

  ASSERT_EQ(restored.keys.size(), 1u);
  ASSERT_EQ(restored.keys.front().values.size(), 1u);
  EXPECT_EQ(restored.keys.front().values.front().type, 0xFFFFFFFFu);
}

TEST(SnapshotJsonTests, RoundTripsNonAsciiNames)
{
  const RegistrySnapshot restored = Read(Write(test::MakeSnapshot(
    { { .path = "HKEY_CURRENT_USER\\Software\\M\xC3\xBCnchen",
        .values = { test::MakeValue("\xE5\xBC\xA0\xE4\xB8\x89", 1, { 0x01 }) } } })));

  ASSERT_EQ(restored.keys.size(), 1u);
  EXPECT_EQ(restored.keys.front().path, "HKEY_CURRENT_USER\\Software\\M\xC3\xBCnchen");
  EXPECT_EQ(restored.keys.front().values.front().name, "\xE5\xBC\xA0\xE4\xB8\x89");
}

TEST(SnapshotJsonTests, ReadsASnapshotWrittenByHand)
{
  const RegistrySnapshot snapshot = Read(kValidSnapshot);

  EXPECT_EQ(snapshot.computer_name, "TESTMACHINE");
  ASSERT_EQ(snapshot.keys.size(), 1u);
  EXPECT_EQ(snapshot.keys.front().path, "HKEY_CURRENT_USER\\Software\\Example");
  EXPECT_EQ(snapshot.keys.front().values.front().data, (std::vector<std::uint8_t>{ 0x45, 0x00 }));
}

TEST(SnapshotJsonTests, PutsASnapshotThatWasNotOrderedIntoCanonicalOrder)
{
  const RegistrySnapshot snapshot = Read(R"({
    "schemaVersion": 1,
    "capturedAt": "",
    "computerName": "",
    "roots": [],
    "diagnostics": [],
    "keys": [
      { "path": "HKEY_CURRENT_USER\\Zulu", "values": [] },
      { "path": "HKEY_CURRENT_USER\\alpha",
        "values": [ { "name": "Second", "type": 1, "data": "" },
                    { "name": "First", "type": 1, "data": "" } ] }
    ]
  })");

  ASSERT_EQ(snapshot.keys.size(), 2u);
  EXPECT_EQ(snapshot.keys[0].path, "HKEY_CURRENT_USER\\alpha");
  EXPECT_EQ(snapshot.keys[1].path, "HKEY_CURRENT_USER\\Zulu");
  EXPECT_EQ(snapshot.keys[0].values[0].name, "First");
  EXPECT_EQ(snapshot.keys[0].values[1].name, "Second");
}

TEST(SnapshotJsonTests, ReportsMalformedJson)
{
  EXPECT_NE(ErrorMessage("{ this is not JSON").find("not valid JSON"), std::string::npos);
}

TEST(SnapshotJsonTests, ReportsAnEmptyFile)
{
  EXPECT_NE(ErrorMessage("").find("not valid JSON"), std::string::npos);
}

TEST(SnapshotJsonTests, ReportsJsonThatIsNotAnObject)
{
  EXPECT_NE(ErrorMessage("[1, 2, 3]").find("not a JSON object"), std::string::npos);
}

TEST(SnapshotJsonTests, ReportsAnUnsupportedSchemaVersion)
{
  const std::string message = ErrorMessage(R"({
    "schemaVersion": 99, "capturedAt": "", "computerName": "",
    "roots": [], "diagnostics": [], "keys": []
  })");

  EXPECT_NE(message.find("schema version 99"), std::string::npos) << message;
}

TEST(SnapshotJsonTests, ReportsAMissingField)
{
  const std::string message = ErrorMessage(R"({
    "schemaVersion": 1, "capturedAt": "", "computerName": "", "roots": [], "diagnostics": []
  })");

  EXPECT_NE(message.find("\"keys\""), std::string::npos) << message;
}

TEST(SnapshotJsonTests, ReportsAFieldOfTheWrongType)
{
  const std::string message = ErrorMessage(R"({
    "schemaVersion": 1, "capturedAt": "", "computerName": "",
    "roots": "not a list", "diagnostics": [], "keys": []
  })");

  EXPECT_NE(message.find("\"roots\""), std::string::npos) << message;
}

TEST(SnapshotJsonTests, ReportsRegistryValueDataThatIsNotHexadecimal)
{
  const std::string message = ErrorMessage(R"({
    "schemaVersion": 1, "capturedAt": "", "computerName": "", "roots": [], "diagnostics": [],
    "keys": [ { "path": "HKEY_CURRENT_USER\\Example",
                "values": [ { "name": "Bad", "type": 1, "data": "zz" } ] } ]
  })");

  EXPECT_NE(message.find("not hexadecimal"), std::string::npos) << message;
}

TEST(SnapshotJsonTests, ReportsRegistryValueDataWithAnOddNumberOfDigits)
{
  const std::string message = ErrorMessage(R"({
    "schemaVersion": 1, "capturedAt": "", "computerName": "", "roots": [], "diagnostics": [],
    "keys": [ { "path": "HKEY_CURRENT_USER\\Example",
                "values": [ { "name": "Bad", "type": 1, "data": "abc" } ] } ]
  })");

  EXPECT_NE(message.find("whole number of hexadecimal bytes"), std::string::npos) << message;
}

TEST(SnapshotJsonTests, ReportsARegistryKeyThatIsNotAnObject)
{
  const std::string message = ErrorMessage(R"({
    "schemaVersion": 1, "capturedAt": "", "computerName": "", "roots": [], "diagnostics": [],
    "keys": [ "HKEY_CURRENT_USER\\Example" ]
  })");

  EXPECT_NE(message.find("not a JSON object"), std::string::npos) << message;
}
