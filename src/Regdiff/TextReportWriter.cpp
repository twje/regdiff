#include "Regdiff/TextReportWriter.h"

// Project
#include "Regdiff/RegistryValueFormatter.h"

// Standard
#include <algorithm>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  // Beyond this, a Registry Value is summarised rather than printed. The whole
  // of the data is in the snapshot; a report is meant to be read.
  constexpr std::size_t kMaximumBytesShown = 32;

  struct ValueTypeName
  {
    std::uint32_t code;
    std::string_view name;
  };

  constexpr ValueTypeName kValueTypeNames[] = {
    { 0, "REG_NONE" },
    { 1, "REG_SZ" },
    { 2, "REG_EXPAND_SZ" },
    { 3, "REG_BINARY" },
    { 4, "REG_DWORD" },
    { 5, "REG_DWORD_BIG_ENDIAN" },
    { 6, "REG_LINK" },
    { 7, "REG_MULTI_SZ" },
    { 8, "REG_RESOURCE_LIST" },
    { 9, "REG_FULL_RESOURCE_DESCRIPTOR" },
    { 10, "REG_RESOURCE_REQUIREMENTS_LIST" },
    { 11, "REG_QWORD" },
  };

  // A Registry Value Type RegDiff does not recognise keeps its number, which is
  // still enough to tell it apart from every other type and to look up.
  std::string DescribeType(std::uint32_t code)
  {
    for (const ValueTypeName& type : kValueTypeNames)
    {
      if (type.code == code)
      {
        return std::string(type.name);
      }
    }

    return "type " + std::to_string(code);
  }

  // Registry Value Data is shown as hexadecimal. That is exact, and it works
  // for every Registry Value Type without RegDiff having to know what any of
  // them mean.
  std::string DescribeData(const std::vector<std::uint8_t>& data)
  {
    if (data.empty())
    {
      return "(empty)";
    }

    const std::size_t shown = std::min(data.size(), kMaximumBytesShown);
    std::string text = ToHexadecimal(std::span<const std::uint8_t>(data.data(), shown));

    if (shown < data.size())
    {
      text += "... (" + std::to_string(data.size()) + " bytes)";
    }

    return text;
  }

  // A Registry Value with an empty Value Name is the default Registry Value.
  std::string_view DisplayName(const std::string& value_name)
  {
    return value_name.empty() ? std::string_view("(Default)") : std::string_view(value_name);
  }

  std::vector<const ValueDifference*> Select(const std::vector<ValueDifference>& differences,
                                             DifferenceType type)
  {
    std::vector<const ValueDifference*> selected;

    for (const ValueDifference& difference : differences)
    {
      if (difference.type == type)
      {
        selected.push_back(&difference);
      }
    }

    return selected;
  }

  void WriteValue(std::ostream& stream, std::string_view label, const RegistryValue& value)
  {
    stream << "    " << label << DescribeType(value.type) << " = " << FormatRegistryValue(value) << '\n';
  }

  void WriteKeySection(std::ostream& stream, std::string_view title, const std::vector<std::string>& key_paths)
  {
    stream << title << " (" << key_paths.size() << "):\n";

    for (const std::string& key_path : key_paths)
    {
      stream << "  " << key_path << '\n';
    }

    stream << '\n';
  }

  void WriteValueSection(std::ostream& stream,
                         std::string_view title,
                         const std::vector<ValueDifference>& differences,
                         DifferenceType type)
  {
    const std::vector<const ValueDifference*> selected = Select(differences, type);
    stream << title << " (" << selected.size() << "):\n";

    for (const ValueDifference* difference : selected)
    {
      stream << "  [" << difference->key_path << "] " << DisplayName(difference->value_name) << '\n';

      if (difference->before.has_value())
      {
        WriteValue(stream, "Before: ", *difference->before);
      }

      if (difference->after.has_value())
      {
        WriteValue(stream, "After:  ", *difference->after);
      }
    }

    stream << '\n';
  }

  void WriteSummary(std::ostream& stream, const ComparisonResult& result)
  {
    stream << "Summary:\n";
    stream << "  Keys added:      " << result.added_keys.size() << '\n';
    stream << "  Keys removed:    " << result.removed_keys.size() << '\n';
    stream << "  Values added:    " << Select(result.value_differences, DifferenceType::kValueAdded).size()
           << '\n';
    stream << "  Values removed:  " << Select(result.value_differences, DifferenceType::kValueRemoved).size()
           << '\n';
    stream << "  Values modified: " << Select(result.value_differences, DifferenceType::kValueModified).size()
           << '\n';
  }
}

void TextReportWriter::Write(std::ostream& stream, const ComparisonResult& result) const
{
  stream << "RegDiff Report\n";
  stream << "==============\n\n";

  if (!result.HasDifferences())
  {
    stream << "No differences found.\n";
    return;
  }

  WriteKeySection(stream, "Keys Added", result.added_keys);
  WriteKeySection(stream, "Keys Removed", result.removed_keys);
  WriteValueSection(stream, "Values Added", result.value_differences, DifferenceType::kValueAdded);
  WriteValueSection(stream, "Values Removed", result.value_differences, DifferenceType::kValueRemoved);
  WriteValueSection(stream, "Values Modified", result.value_differences, DifferenceType::kValueModified);
  WriteSummary(stream, result);
}
