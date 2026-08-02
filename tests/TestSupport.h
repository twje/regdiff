#pragma once

// Project
#include "Regdiff/RegistryAccess.h"
#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test
{
  // An in-memory Windows Registry.
  //
  // A test lists only the Registry Keys it cares about, by full path. Which
  // Registry Key is a subkey of which is worked out from those paths, so there
  // is no tree to build by hand.
  class FakeRegistry : public RegistryAccess
  {
  public:
    FakeRegistry& AddKey(std::string path, std::vector<RegistryValue> values = {})
    {
      keys_.insert_or_assign(std::move(path), std::move(values));
      return *this;
    }

    // Makes a Registry Key fail to open, the way an unreadable one does on a
    // real machine. It is still listed as a subkey of its parent.
    FakeRegistry& DenyKey(std::string path, std::string error = "access is denied")
    {
      denied_.insert_or_assign(std::move(path), std::move(error));
      return *this;
    }

    RegistryReadResult Read(std::string_view path) const override
    {
      RegistryReadResult result;
      const std::string key(path);

      if (const auto denied = denied_.find(key); denied != denied_.end())
      {
        result.error = denied->second;
        return result;
      }

      const auto entry = keys_.find(key);
      if (entry == keys_.end())
      {
        result.error = "the Registry Key does not exist";
        return result;
      }

      result.opened = true;
      result.values = entry->second;

      const std::string prefix = key + '\\';
      CollectChildren(keys_, prefix, result.subkey_names);
      CollectChildren(denied_, prefix, result.subkey_names);

      return result;
    }

  private:
    template <typename Map>
    static void CollectChildren(const Map& source, const std::string& prefix, std::vector<std::string>& names)
    {
      for (const auto& [path, ignored] : source)
      {
        if (path.starts_with(prefix) && path.find('\\', prefix.size()) == std::string::npos)
        {
          names.push_back(path.substr(prefix.size()));
        }
      }
    }

    std::map<std::string, std::vector<RegistryValue>> keys_;
    std::map<std::string, std::string> denied_;
  };

  inline RegistryValue MakeValue(std::string name, std::uint32_t type, std::vector<std::uint8_t> data)
  {
    return { .name = std::move(name), .type = type, .data = std::move(data) };
  }

  // A REG_SZ Registry Value holding ASCII text, encoded the way Windows stores
  // it: UTF-16 LE with a terminator.
  inline RegistryValue MakeStringValue(std::string name, std::string_view text)
  {
    std::vector<std::uint8_t> data;

    for (const char character : text)
    {
      data.push_back(static_cast<std::uint8_t>(character));
      data.push_back(0);
    }

    data.push_back(0);
    data.push_back(0);

    return MakeValue(std::move(name), 1, std::move(data));
  }

  inline RegistrySnapshot MakeSnapshot(std::vector<RegistryKey> keys)
  {
    RegistrySnapshot snapshot;
    snapshot.captured_at = "2026-08-02T09:15:00Z";
    snapshot.computer_name = "TESTMACHINE";
    snapshot.keys = std::move(keys);

    Canonicalise(snapshot);
    return snapshot;
  }

  // Writes bytes to a uniquely named file in the temporary directory and
  // removes the file again when the test finishes with it.
  class TemporaryFile
  {
  public:
    explicit TemporaryFile(std::string_view file_name, std::string_view bytes = {})
      : path_(std::filesystem::temp_directory_path() / file_name)
    {
      std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
      if (!bytes.empty())
      {
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
      }
    }

    ~TemporaryFile()
    {
      std::error_code ignored;
      std::filesystem::remove(path_, ignored);
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    const std::filesystem::path& Path() const { return path_; }

    std::string Name() const { return path_.string(); }

    std::string Contents() const
    {
      std::ifstream stream(path_, std::ios::binary);
      return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    }

  private:
    std::filesystem::path path_;
  };
}
