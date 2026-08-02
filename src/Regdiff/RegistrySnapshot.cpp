#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <algorithm>
#include <iterator>

namespace
{
  struct RegistryRoot
  {
    std::string_view name;
    std::string_view abbreviation;
  };

  // The standard Windows Registry hives. This is the only place the two
  // spellings of a Registry root are written down: --all captures them in this
  // order, and ExpandRegistryRoot accepts either spelling of each.
  constexpr RegistryRoot kRegistryRoots[] = {
    { "HKEY_CLASSES_ROOT", "HKCR" },
    { "HKEY_CURRENT_USER", "HKCU" },
    { "HKEY_LOCAL_MACHINE", "HKLM" },
    { "HKEY_USERS", "HKU" },
    { "HKEY_CURRENT_CONFIG", "HKCC" },
  };

  constexpr char ToLowerAscii(char character)
  {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
  }

  bool OrderedByPath(const RegistryKey& left, const RegistryKey& right)
  {
    return CompareRegistryNames(left.path, right.path) < 0;
  }

  bool SamePath(const RegistryKey& left, const RegistryKey& right)
  {
    return CompareRegistryNames(left.path, right.path) == 0;
  }

  bool OrderedByName(const RegistryValue& left, const RegistryValue& right)
  {
    return CompareRegistryNames(left.name, right.name) < 0;
  }
}

int CompareRegistryNames(std::string_view left, std::string_view right)
{
  const std::size_t shared = std::min(left.size(), right.size());

  for (std::size_t index = 0; index < shared; ++index)
  {
    const auto left_character = static_cast<unsigned char>(ToLowerAscii(left[index]));
    const auto right_character = static_cast<unsigned char>(ToLowerAscii(right[index]));

    if (left_character != right_character)
    {
      return left_character < right_character ? -1 : 1;
    }
  }

  if (left.size() == right.size())
  {
    return 0;
  }

  return left.size() < right.size() ? -1 : 1;
}

void Canonicalise(RegistrySnapshot& snapshot)
{
  std::ranges::sort(snapshot.keys, OrderedByPath);

  const auto duplicates = std::ranges::unique(snapshot.keys, SamePath);
  snapshot.keys.erase(duplicates.begin(), duplicates.end());

  for (RegistryKey& key : snapshot.keys)
  {
    std::ranges::sort(key.values, OrderedByName);
  }

  std::ranges::sort(snapshot.diagnostics);
}

std::string ToHexadecimal(std::span<const std::uint8_t> data)
{
  constexpr std::string_view kDigits = "0123456789abcdef";

  std::string text;
  text.reserve(data.size() * 2);

  for (const std::uint8_t byte : data)
  {
    text.push_back(kDigits[byte >> 4]);
    text.push_back(kDigits[byte & 0x0F]);
  }

  return text;
}

std::vector<std::string> StandardRegistryRoots()
{
  std::vector<std::string> roots;
  roots.reserve(std::size(kRegistryRoots));

  for (const RegistryRoot& root : kRegistryRoots)
  {
    roots.emplace_back(root.name);
  }

  return roots;
}

std::string ExpandRegistryRoot(std::string_view path)
{
  const std::size_t separator = path.find('\\');
  const std::string_view head = path.substr(0, separator);

  for (const RegistryRoot& root : kRegistryRoots)
  {
    if (CompareRegistryNames(head, root.name) != 0 && CompareRegistryNames(head, root.abbreviation) != 0)
    {
      continue;
    }

    std::string expanded(root.name);
    if (separator != std::string_view::npos)
    {
      expanded += path.substr(separator);  // Keeps the separator and the case below it.
    }

    return expanded;
  }

  return std::string(path);
}
