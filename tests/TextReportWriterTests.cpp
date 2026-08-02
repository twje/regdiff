// Project
#include "Regdiff/TextReportWriter.h"

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
  std::string Report(const ComparisonResult& result)
  {
    std::ostringstream stream;
    TextReportWriter().Write(stream, result);

    return stream.str();
  }

  ValueDifference Modified(std::string name, RegistryValue before, RegistryValue after)
  {
    return { .type = DifferenceType::kValueModified,
             .key_path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Example",
             .value_name = std::move(name),
             .before = std::move(before),
             .after = std::move(after) };
  }
}

TEST(TextReportWriterTests, ReportsThatThereAreNoDifferences)
{
  const std::string report = Report({});

  EXPECT_NE(report.find("No differences found."), std::string::npos) << report;
  EXPECT_EQ(report.find("Summary:"), std::string::npos) << report;
}

TEST(TextReportWriterTests, ListsAddedAndRemovedRegistryKeys)
{
  ComparisonResult result;
  result.added_keys = { "HKEY_LOCAL_MACHINE\\SOFTWARE\\New" };
  result.removed_keys = { "HKEY_LOCAL_MACHINE\\SOFTWARE\\Old" };

  const std::string report = Report(result);

  EXPECT_NE(report.find("Keys Added (1):"), std::string::npos) << report;
  EXPECT_NE(report.find("HKEY_LOCAL_MACHINE\\SOFTWARE\\New"), std::string::npos) << report;
  EXPECT_NE(report.find("Keys Removed (1):"), std::string::npos) << report;
  EXPECT_NE(report.find("HKEY_LOCAL_MACHINE\\SOFTWARE\\Old"), std::string::npos) << report;
}

TEST(TextReportWriterTests, NamesTheRegistryKeyAndValueThatChanged)
{
  ComparisonResult result;
  result.value_differences = {
    Modified("Version", test::MakeValue("Version", 1, { 0x01 }), test::MakeValue("Version", 1, { 0x02 }))
  };

  EXPECT_NE(Report(result).find("[HKEY_LOCAL_MACHINE\\SOFTWARE\\Example] Version"), std::string::npos);
}

TEST(TextReportWriterTests, ShowsRegistryValueTypeNames)
{
  ComparisonResult result;
  result.value_differences = {
    Modified("Enabled", test::MakeValue("Enabled", 4, { 0x00 }), test::MakeValue("Enabled", 11, { 0x01 }))
  };

  const std::string report = Report(result);

  EXPECT_NE(report.find("Before: REG_DWORD = 00"), std::string::npos) << report;
  EXPECT_NE(report.find("After:  REG_QWORD = 01"), std::string::npos) << report;
}

TEST(TextReportWriterTests, ShowsTheNumberOfAnUnrecognisedRegistryValueType)
{
  ComparisonResult result;
  result.value_differences = {
    Modified("Odd", test::MakeValue("Odd", 0x200000, { 0x01 }), test::MakeValue("Odd", 0x200000, { 0x02 }))
  };

  const std::string report = Report(result);

  EXPECT_NE(report.find("type 2097152"), std::string::npos) << report;
}

TEST(TextReportWriterTests, ShowsAnEmptyRegistryValueAsEmpty)
{
  ComparisonResult result;
  result.value_differences = {
    Modified("Blank", test::MakeValue("Blank", 0, {}), test::MakeValue("Blank", 0, { 0x01 }))
  };

  const std::string report = Report(result);

  EXPECT_NE(report.find("Before: REG_NONE = (empty)"), std::string::npos) << report;
}

TEST(TextReportWriterTests, ShortensLongRegistryValueData)
{
  ComparisonResult result;
  result.value_differences = { Modified("Blob",
                                        test::MakeValue("Blob", 3, std::vector<std::uint8_t>(100, 0xAB)),
                                        test::MakeValue("Blob", 3, { 0x01 })) };

  const std::string report = Report(result);

  // Thirty-two bytes of 0xAB are shown as "ab" apiece, then the size of the
  // whole of the data.
  std::string shown;
  for (int byte = 0; byte < 32; ++byte)
  {
    shown += "ab";
  }

  EXPECT_NE(report.find("REG_BINARY = " + shown + "... (100 bytes)"), std::string::npos) << report;
}

TEST(TextReportWriterTests, NamesTheDefaultRegistryValue)
{
  ComparisonResult result;
  result.value_differences = {
    Modified("", test::MakeValue("", 1, { 0x01 }), test::MakeValue("", 1, { 0x02 }))
  };

  const std::string report = Report(result);

  EXPECT_NE(report.find("(Default)"), std::string::npos) << report;
}

TEST(TextReportWriterTests, ShowsOnlyTheSideThatExistsForAnAddedRegistryValue)
{
  ComparisonResult result;
  result.value_differences = { { .type = DifferenceType::kValueAdded,
                                 .key_path = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Example",
                                 .value_name = "Added",
                                 .after = test::MakeValue("Added", 1, { 0x01 }) } };

  const std::string report = Report(result);

  EXPECT_NE(report.find("After:"), std::string::npos) << report;
  EXPECT_EQ(report.find("Before:"), std::string::npos) << report;
}

TEST(TextReportWriterTests, CountsEveryKindOfDifferenceInTheSummary)
{
  ComparisonResult result;
  result.added_keys = { "HKEY_LOCAL_MACHINE\\SOFTWARE\\New" };
  result.removed_keys = { "HKEY_LOCAL_MACHINE\\SOFTWARE\\Old" };
  result.value_differences = {
    { .type = DifferenceType::kValueAdded, .value_name = "A", .after = test::MakeValue("A", 1, { 0x01 }) },
    { .type = DifferenceType::kValueRemoved, .value_name = "R", .before = test::MakeValue("R", 1, { 0x01 }) },
    Modified("M", test::MakeValue("M", 1, { 0x01 }), test::MakeValue("M", 1, { 0x02 }))
  };

  const std::string report = Report(result);

  EXPECT_NE(report.find("Keys added:      1"), std::string::npos) << report;
  EXPECT_NE(report.find("Keys removed:    1"), std::string::npos) << report;
  EXPECT_NE(report.find("Values added:    1"), std::string::npos) << report;
  EXPECT_NE(report.find("Values removed:  1"), std::string::npos) << report;
  EXPECT_NE(report.find("Values modified: 1"), std::string::npos) << report;
}
