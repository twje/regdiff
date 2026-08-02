#pragma once

// Project
#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <optional>
#include <string>
#include <vector>

enum class DifferenceType
{
  kValueAdded,
  kValueRemoved,
  kValueModified
};

// Describes a Registry Value that was added, removed, or modified.
//
// before is populated unless the value was added.
// after is populated unless the value was removed.
struct ValueDifference
{
  DifferenceType type = DifferenceType::kValueModified;

  std::string key_path;
  std::string value_name;

  std::optional<RegistryValue> before;
  std::optional<RegistryValue> after;
};

struct ComparisonResult
{
  std::vector<std::string> added_keys;
  std::vector<std::string> removed_keys;
  std::vector<ValueDifference> value_differences;

  bool HasDifferences() const
  {
    return !added_keys.empty()
      || !removed_keys.empty()
      || !value_differences.empty();
  }
};

// Compares two Registry snapshots.
//
// Registry Key paths and Value Names are matched case-insensitively to match
// Windows behaviour. Registry Value Types and Value Data must match exactly.
//
// Added and removed Registry Keys are reported once. Their contained Registry
// Values are not reported individually, as they are implied by the Registry Key
// change.
class RegistryComparator
{
public:
  ComparisonResult Compare(const RegistrySnapshot& before, const RegistrySnapshot& after) const;
};