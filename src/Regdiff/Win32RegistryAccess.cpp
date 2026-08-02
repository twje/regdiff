#include "Regdiff/RegistryAccess.h"

// Standard
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>

// Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace
{
  struct RegistryRoot
  {
    std::string_view name;
    HKEY key;
  };

  // Full names only. Abbreviations are expanded by ExpandRegistryRoot before a
  // path gets here, so they are written down in one place rather than two. The
  // HKEY constants are not constant expressions, so this is an ordinary array
  // rather than a constexpr one.
  const RegistryRoot kRoots[] = {
    { "HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
    { "HKEY_CURRENT_USER", HKEY_CURRENT_USER },
    { "HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE },
    { "HKEY_USERS", HKEY_USERS },
    { "HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG },
  };

  std::wstring ToUtf16(std::string_view text)
  {
    if (text.empty())
    {
      return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);

    return wide;
  }

  std::string ToUtf8(std::wstring_view text)
  {
    if (text.empty())
    {
      return {};
    }

    const int length = WideCharToMultiByte(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length, nullptr, nullptr);

    return utf8;
  }

  // Splits "HKEY_CURRENT_USER\\Software\\Example" into the Registry root and
  // the path below it. Returns nullptr when the leading name is not a root.
  HKEY SplitRoot(std::string_view path, std::string_view& subpath)
  {
    const std::size_t separator = path.find('\\');

    subpath = separator == std::string_view::npos ? std::string_view() : path.substr(separator + 1);

    const std::string_view root_name = path.substr(0, separator);
    for (const RegistryRoot& root : kRoots)
    {
      if (CompareRegistryNames(root.name, root_name) == 0)
      {
        return root.key;
      }
    }

    return nullptr;
  }

  // The handful of failures worth naming. Anything else keeps its number, which
  // is enough to look up and keeps the message independent of the system locale.
  std::string Describe(LSTATUS status)
  {
    switch (status)
    {
      case ERROR_ACCESS_DENIED:
        return "access is denied";
      case ERROR_FILE_NOT_FOUND:
        return "the Registry Key does not exist";
      default:
        return "Windows error " + std::to_string(status);
    }
  }

  // Closes a Registry Key handle when it goes out of scope.
  class ScopedKey
  {
  public:
    explicit ScopedKey(HKEY key)
      : key_(key)
    {
    }

    ~ScopedKey()
    {
      RegCloseKey(key_);
    }

    ScopedKey(const ScopedKey&) = delete;
    ScopedKey& operator=(const ScopedKey&) = delete;

    HKEY Get() const { return key_; }

  private:
    HKEY key_;
  };

  // Enumeration stops at the first failure rather than trusting the count from
  // RegQueryInfoKeyW: the Registry is live, so subkeys can come and go while it
  // is being read. The count is still worth having to size the result.
  void ReadSubkeyNames(HKEY key, DWORD longest_name, std::vector<std::string>& names)
  {
    std::wstring name(static_cast<std::size_t>(longest_name) + 1, L'\0');

    for (DWORD index = 0;; ++index)
    {
      DWORD length = static_cast<DWORD>(name.size());

      if (RegEnumKeyExW(key, index, name.data(), &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
      {
        return;
      }

      names.push_back(ToUtf8({ name.data(), length }));
    }
  }

  void ReadValues(HKEY key, DWORD longest_name, DWORD largest_data, std::vector<RegistryValue>& values)
  {
    std::wstring name(static_cast<std::size_t>(longest_name) + 1, L'\0');

    // Never empty, so that data() is a usable pointer even for a Registry Key
    // whose Registry Values all hold nothing.
    std::vector<std::uint8_t> data(std::max<std::size_t>(largest_data, 1));

    for (DWORD index = 0;; ++index)
    {
      DWORD name_length = static_cast<DWORD>(name.size());
      DWORD data_length = static_cast<DWORD>(data.size());
      DWORD type = REG_NONE;

      LSTATUS status =
        RegEnumValueW(key, index, name.data(), &name_length, nullptr, &type, data.data(), &data_length);

      if (status == ERROR_MORE_DATA)
      {
        // A Registry Value can grow between being measured and being read.
        // Windows reports the size it now needs.
        data.resize(data_length);

        name_length = static_cast<DWORD>(name.size());
        data_length = static_cast<DWORD>(data.size());

        status =
          RegEnumValueW(key, index, name.data(), &name_length, nullptr, &type, data.data(), &data_length);
      }

      if (status != ERROR_SUCCESS)
      {
        return;
      }

      RegistryValue value;
      value.name = ToUtf8({ name.data(), name_length });
      value.type = static_cast<std::uint32_t>(type);
      value.data.assign(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(data_length));

      values.push_back(std::move(value));
    }
  }
}

RegistryReadResult Win32RegistryAccess::Read(std::string_view path) const
{
  RegistryReadResult result;

  // Expanded here as well as on the command line, so that reading a Registry
  // Key does not depend on who asked for it.
  const std::string full_path = ExpandRegistryRoot(path);

  std::string_view subpath;
  const HKEY root = SplitRoot(full_path, subpath);
  if (root == nullptr)
  {
    result.error = "the path does not start with a Registry root such as HKEY_LOCAL_MACHINE";
    return result;
  }

  HKEY opened = nullptr;
  const LSTATUS status = RegOpenKeyExW(root, ToUtf16(subpath).c_str(), 0, KEY_READ, &opened);
  if (status != ERROR_SUCCESS)
  {
    result.error = Describe(status);
    return result;
  }

  const ScopedKey key(opened);

  DWORD subkey_count = 0;
  DWORD longest_subkey_name = 0;
  DWORD value_count = 0;
  DWORD longest_value_name = 0;
  DWORD largest_value_data = 0;

  const LSTATUS queried = RegQueryInfoKeyW(key.Get(),
                                           nullptr,
                                           nullptr,
                                           nullptr,
                                           &subkey_count,
                                           &longest_subkey_name,
                                           nullptr,
                                           &value_count,
                                           &longest_value_name,
                                           &largest_value_data,
                                           nullptr,
                                           nullptr);
  if (queried != ERROR_SUCCESS)
  {
    result.error = Describe(queried);
    return result;
  }

  result.opened = true;
  result.subkey_names.reserve(subkey_count);
  result.values.reserve(value_count);

  ReadSubkeyNames(key.Get(), longest_subkey_name, result.subkey_names);
  ReadValues(key.Get(), longest_value_name, largest_value_data, result.values);

  return result;
}

std::string LocalComputerName()
{
  wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD length = static_cast<DWORD>(std::size(name));

  if (GetComputerNameW(name, &length) == 0)
  {
    return {};
  }

  return ToUtf8({ name, length });
}
