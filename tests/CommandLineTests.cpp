// Project
#include "CommandLine.h"
#include "Regdiff/SnapshotJson.h"

// Tests
#include "TestSupport.h"

// Third party
#include <gtest/gtest.h>

// Standard
#include <sstream>
#include <string>
#include <vector>

namespace
{
  // What running RegDiff produced. Not named Run: testing::Test has a private
  // member function of that name, which would hide this one inside a test body.
  struct CommandResult
  {
    int exit_code = 0;
    std::string out;
    std::string err;
  };

  CommandResult RunRegDiff(std::vector<std::string> arguments)
  {
    std::vector<const char*> argv{ "RegDiff" };
    for (const std::string& argument : arguments)
    {
      argv.push_back(argument.c_str());
    }

    std::ostringstream out;
    std::ostringstream err;

    CommandResult result;
    result.exit_code = RunCommandLine(static_cast<int>(argv.size()), argv.data(), out, err);
    result.out = out.str();
    result.err = err.str();

    return result;
  }

  std::string SnapshotJson(const RegistrySnapshot& snapshot)
  {
    std::ostringstream stream;
    SnapshotWriter().Write(stream, snapshot);

    return stream.str();
  }

  RegistrySnapshot ExampleSnapshot(std::string_view data)
  {
    return test::MakeSnapshot({ { .path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Example",
                                  .values = { test::MakeStringValue("Version", data) } } });
  }

  // A Registry Key that exists in every Windows user profile. The snapshot
  // tests read it and nothing else; RegDiff never writes to the Registry.
  constexpr std::string_view kReadableRoot = "HKEY_CURRENT_USER\\Environment";
}

TEST(CommandLineTests, ReportsAnUnknownCommand)
{
  EXPECT_EQ(RunRegDiff({ "explode" }).exit_code, 2);
}

TEST(CommandLineTests, ReportsNoCommandAtAll)
{
  EXPECT_EQ(RunRegDiff({}).exit_code, 2);
}

TEST(CommandLineTests, PrintsHelpSuccessfully)
{
  const CommandResult result = RunRegDiff({ "--help" });

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_NE(result.out.find("snapshot"), std::string::npos) << result.out;
  EXPECT_NE(result.out.find("compare"), std::string::npos) << result.out;
}

TEST(CommandLineTests, ReportsSnapshotWithNeitherARootNorAll)
{
  const test::TemporaryFile output("regdiff_missing_root.json");

  const CommandResult result = RunRegDiff({ "snapshot", "--output", output.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_NE(result.err.find("nothing to capture"), std::string::npos) << result.err;
}

TEST(CommandLineTests, RejectsAllAndRootTogether)
{
  const test::TemporaryFile output("regdiff_all_and_root.json");

  // Rejected while the command line is parsed, so nothing is captured.
  const CommandResult result =
    RunRegDiff({ "snapshot", "--all", "--root", "HKLM", "--output", output.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_TRUE(output.Contents().empty()) << output.Contents();
}

TEST(CommandLineTests, DocumentsBothWaysOfChoosingWhatToCapture)
{
  const CommandResult result = RunRegDiff({ "snapshot", "--help" });

  ASSERT_EQ(result.exit_code, 0);
  EXPECT_NE(result.out.find("-a,--all"), std::string::npos) << result.out;
  EXPECT_NE(result.out.find("Capture the standard Windows Registry hives."), std::string::npos)
    << result.out;
  EXPECT_NE(result.out.find("-r,--root TEXT"), std::string::npos) << result.out;
  EXPECT_NE(result.out.find("May be specified multiple times."), std::string::npos) << result.out;
  EXPECT_NE(result.out.find("HKEY_LOCAL_MACHINE\\SOFTWARE"), std::string::npos) << result.out;
  EXPECT_NE(result.out.find("-o,--output FILE"), std::string::npos) << result.out;
}

TEST(CommandLineTests, CapturesAnAbbreviatedRegistryRootUnderItsFullName)
{
  const test::TemporaryFile output("regdiff_abbreviated.json");

  const CommandResult result =
    RunRegDiff({ "snapshot", "--root", "HKCU\\Environment", "--output", output.Name() });

  ASSERT_EQ(result.exit_code, 0) << result.err;

  std::istringstream written(output.Contents());
  const RegistrySnapshot snapshot = SnapshotReader().Read(written);

  ASSERT_EQ(snapshot.roots.size(), 1u);
  EXPECT_EQ(snapshot.roots.front(), "HKEY_CURRENT_USER\\Environment");
  EXPECT_EQ(snapshot.keys.front().path, "HKEY_CURRENT_USER\\Environment");
}

TEST(CommandLineTests, ExpandsAnAbbreviatedRegistryRootBeforeReportingIt)
{
  const test::TemporaryFile output("regdiff_abbreviated_bad.json");

  const CommandResult result = RunRegDiff(
    { "snapshot", "--root", "HKCU\\RegDiffKeyThatDoesNotExist", "--output", output.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_NE(result.err.find("HKEY_CURRENT_USER\\RegDiffKeyThatDoesNotExist"), std::string::npos)
    << result.err;
}

TEST(CommandLineTests, ReportsSnapshotWithoutAnOutputFile)
{
  EXPECT_EQ(RunRegDiff({ "snapshot", "--root", std::string(kReadableRoot) }).exit_code, 2);
}

TEST(CommandLineTests, CapturesASnapshotOfTheLiveRegistry)
{
  const test::TemporaryFile output("regdiff_snapshot.json");

  const CommandResult result =
    RunRegDiff({ "snapshot", "--root", std::string(kReadableRoot), "--output", output.Name() });

  ASSERT_EQ(result.exit_code, 0) << result.err;
  EXPECT_NE(result.err.find("Captured"), std::string::npos) << result.err;

  // Whatever happens to be in that Registry Key, the file it wrote has to be a
  // snapshot this build can read back.
  std::istringstream written(output.Contents());
  const RegistrySnapshot snapshot = SnapshotReader().Read(written);

  EXPECT_EQ(snapshot.schema_version, kSnapshotSchemaVersion);
  EXPECT_FALSE(snapshot.keys.empty());
  EXPECT_FALSE(snapshot.captured_at.empty());
  ASSERT_EQ(snapshot.roots.size(), 1u);
  EXPECT_EQ(snapshot.roots.front(), kReadableRoot);
}

TEST(CommandLineTests, ReportsARootThatCannotBeRead)
{
  const test::TemporaryFile output("regdiff_bad_root.json");

  const CommandResult result = RunRegDiff(
    { "snapshot", "--root", "HKEY_CURRENT_USER\\RegDiffKeyThatDoesNotExist", "--output", output.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_NE(result.err.find("no Registry Keys could be read"), std::string::npos) << result.err;
}

TEST(CommandLineTests, ReportsAPathThatIsNotARegistryRoot)
{
  const test::TemporaryFile output("regdiff_no_hive.json");

  const CommandResult result =
    RunRegDiff({ "snapshot", "--root", "NOT_A_HIVE\\Example", "--output", output.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_NE(result.err.find("Registry root"), std::string::npos) << result.err;
}

TEST(CommandLineTests, ComparesTwoIdenticalSnapshots)
{
  const std::string json = SnapshotJson(ExampleSnapshot("1.0"));
  const test::TemporaryFile before("regdiff_same_before.json", json);
  const test::TemporaryFile after("regdiff_same_after.json", json);

  const CommandResult result = RunRegDiff({ "compare", before.Name(), after.Name() });

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_NE(result.out.find("No differences found."), std::string::npos) << result.out;
}

TEST(CommandLineTests, ComparesTwoSnapshotsThatDiffer)
{
  const test::TemporaryFile before("regdiff_diff_before.json", SnapshotJson(ExampleSnapshot("1.0")));
  const test::TemporaryFile after("regdiff_diff_after.json", SnapshotJson(ExampleSnapshot("2.0")));

  const CommandResult result = RunRegDiff({ "compare", before.Name(), after.Name() });

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_NE(result.out.find("Values modified: 1"), std::string::npos) << result.out;
}

TEST(CommandLineTests, ReportsASnapshotFileThatIsMissing)
{
  const test::TemporaryFile before("regdiff_present.json", SnapshotJson(ExampleSnapshot("1.0")));

  EXPECT_EQ(RunRegDiff({ "compare", before.Name(), "regdiff_no_such_file.json" }).exit_code, 2);
}

TEST(CommandLineTests, ReportsASnapshotFileThatIsMalformed)
{
  const test::TemporaryFile before("regdiff_broken.json", "{ not json");
  const test::TemporaryFile after("regdiff_fine.json", SnapshotJson(ExampleSnapshot("1.0")));

  const CommandResult result = RunRegDiff({ "compare", before.Name(), after.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_NE(result.err.find("not valid JSON"), std::string::npos) << result.err;
  EXPECT_NE(result.err.find(before.Name()), std::string::npos) << result.err;
}

TEST(CommandLineTests, ReportsASnapshotWrittenToAnotherSchemaVersion)
{
  const test::TemporaryFile before("regdiff_future.json", R"({
    "schemaVersion": 99, "capturedAt": "", "computerName": "",
    "roots": [], "diagnostics": [], "keys": []
  })");
  const test::TemporaryFile after("regdiff_current.json", SnapshotJson(ExampleSnapshot("1.0")));

  const CommandResult result = RunRegDiff({ "compare", before.Name(), after.Name() });

  EXPECT_EQ(result.exit_code, 2);
  EXPECT_NE(result.err.find("schema version 99"), std::string::npos) << result.err;
}
