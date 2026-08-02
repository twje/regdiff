#include "Regdiff/RegistryComparator.h"

namespace
{
  bool ValuesDiffer(const RegistryValue& before, const RegistryValue& after)
  {
    return before.type != after.type
      || before.data != after.data;
  }

  // Returns the ordering of the next items, treating an exhausted sequence as
  // sorting after any remaining items.
  int NextOrder(int order, bool has_left, bool has_right)
  {
    if (!has_right)
    {
      return -1;
    }

    if (!has_left)
    {
      return 1;
    }

    return order;
  }

  // Matching Registry Keys are compared by walking their sorted Registry Values
  // in lockstep.
  void CompareValues(
    const RegistryKey& before,
    const RegistryKey& after,
    std::vector<ValueDifference>& differences)
  {
    std::size_t left = 0;
    std::size_t right = 0;

    while (left < before.values.size() || right < after.values.size())
    {
      const bool has_left = left < before.values.size();
      const bool has_right = right < after.values.size();

      int order = 0;

      if (has_left && has_right)
      {
        order = CompareRegistryNames(
          before.values[left].name,
          after.values[right].name);
      }

      order = NextOrder(order, has_left, has_right);

      if (order < 0)
      {
        const RegistryValue& value = before.values[left++];

        differences.push_back({
          .type = DifferenceType::kValueRemoved,
          .key_path = before.path,
          .value_name = value.name,
          .before = value
          });
      }
      else if (order > 0)
      {
        const RegistryValue& value = after.values[right++];

        differences.push_back({
          .type = DifferenceType::kValueAdded,
          .key_path = before.path,
          .value_name = value.name,
          .after = value
          });
      }
      else
      {
        const RegistryValue& was = before.values[left++];
        const RegistryValue& now = after.values[right++];

        if (ValuesDiffer(was, now))
        {
          differences.push_back({
            .type = DifferenceType::kValueModified,
            .key_path = before.path,
            .value_name = was.name,
            .before = was,
            .after = now
            });
        }
      }
    }
  }
}

ComparisonResult RegistryComparator::Compare(
  const RegistrySnapshot& before,
  const RegistrySnapshot& after) const
{
  ComparisonResult result;

  // Both snapshots are sorted by Registry Key path, allowing them to be walked
  // in lockstep. Keys present in only one snapshot were added or removed;
  // matching Registry Keys are compared for value differences.
  std::size_t left = 0;
  std::size_t right = 0;

  while (left < before.keys.size() || right < after.keys.size())
  {
    const bool has_left = left < before.keys.size();
    const bool has_right = right < after.keys.size();

    int order = 0;

    if (has_left && has_right)
    {
      order = CompareRegistryNames(
        before.keys[left].path,
        after.keys[right].path);
    }

    order = NextOrder(order, has_left, has_right);

    if (order < 0)
    {
      result.removed_keys.push_back(before.keys[left++].path);
    }
    else if (order > 0)
    {
      result.added_keys.push_back(after.keys[right++].path);
    }
    else
    {
      CompareValues(
        before.keys[left],
        after.keys[right],
        result.value_differences);

      ++left;
      ++right;
    }
  }

  return result;
}