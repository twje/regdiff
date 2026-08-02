#include "Regdiff/SnapshotJson.h"

// Project
#include "Regdiff/RegistryError.h"

// Third party
#include <nlohmann/json.hpp>

// Standard
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using Json = nlohmann::json;

  // Only the reader needs to decode hexadecimal; the writer shares
  // ToHexadecimal with the report so that both spell bytes the same way.
  int HexDigitValue(char character)
  {
    if (character >= '0' && character <= '9')
    {
      return character - '0';
    }

    if (character >= 'a' && character <= 'f')
    {
      return character - 'a' + 10;
    }

    if (character >= 'A' && character <= 'F')
    {
      return character - 'A' + 10;
    }

    return -1;
  }

  std::vector<std::uint8_t> FromHexadecimal(std::string_view text)
  {
    if (text.size() % 2 != 0)
    {
      throw RegistryError("Registry Value data is not a whole number of hexadecimal bytes");
    }

    std::vector<std::uint8_t> data;
    data.reserve(text.size() / 2);

    for (std::size_t index = 0; index < text.size(); index += 2)
    {
      const int high = HexDigitValue(text[index]);
      const int low = HexDigitValue(text[index + 1]);

      if (high < 0 || low < 0)
      {
        throw RegistryError("Registry Value data is not hexadecimal");
      }

      data.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }

    return data;
  }

  // The field readers below name the field they were looking for. The
  // exceptions nlohmann throws do not, and a snapshot is something a person may
  // well have edited by hand.
  const Json& Field(const Json& parent, std::string_view name)
  {
    // nlohmann keys objects by std::string and its comparator is not
    // transparent, so the name is converted rather than looked up as a view.
    const auto field = parent.find(std::string(name));
    if (field == parent.end())
    {
      throw RegistryError("the snapshot has no \"" + std::string(name) + "\" field");
    }

    return *field;
  }

  [[noreturn]] void ThrowWrongType(std::string_view name, std::string_view expected)
  {
    throw RegistryError("the snapshot field \"" + std::string(name) + "\" is not " + std::string(expected));
  }

  std::string ReadString(const Json& parent, std::string_view name)
  {
    const Json& field = Field(parent, name);
    if (!field.is_string())
    {
      ThrowWrongType(name, "text");
    }

    return field.get<std::string>();
  }

  std::uint32_t ReadNumber(const Json& parent, std::string_view name)
  {
    const Json& field = Field(parent, name);
    if (!field.is_number_unsigned())
    {
      ThrowWrongType(name, "a whole number that is not negative");
    }

    return field.get<std::uint32_t>();
  }

  const Json& ReadArray(const Json& parent, std::string_view name)
  {
    const Json& field = Field(parent, name);
    if (!field.is_array())
    {
      ThrowWrongType(name, "a list");
    }

    return field;
  }

  std::vector<std::string> ReadStrings(const Json& parent, std::string_view name)
  {
    std::vector<std::string> strings;

    for (const Json& element : ReadArray(parent, name))
    {
      if (!element.is_string())
      {
        ThrowWrongType(name, "a list of text");
      }

      strings.push_back(element.get<std::string>());
    }

    return strings;
  }

  void ReadSchemaVersion(const Json& document)
  {
    const Json& field = Field(document, "schemaVersion");
    if (!field.is_number_integer())
    {
      ThrowWrongType("schemaVersion", "a whole number");
    }

    const int version = field.get<int>();
    if (version != kSnapshotSchemaVersion)
    {
      throw RegistryError("the snapshot uses schema version " + std::to_string(version) +
                          ", and this build of RegDiff reads version " +
                          std::to_string(kSnapshotSchemaVersion));
    }
  }

  RegistryValue ReadValue(const Json& element)
  {
    if (!element.is_object())
    {
      throw RegistryError("a Registry Value in the snapshot is not a JSON object");
    }

    return { .name = ReadString(element, "name"),
             .type = ReadNumber(element, "type"),
             .data = FromHexadecimal(ReadString(element, "data")) };
  }

  RegistryKey ReadKey(const Json& element)
  {
    if (!element.is_object())
    {
      throw RegistryError("a Registry Key in the snapshot is not a JSON object");
    }

    RegistryKey key;
    key.path = ReadString(element, "path");

    for (const Json& value : ReadArray(element, "values"))
    {
      key.values.push_back(ReadValue(value));
    }

    return key;
  }
}

void SnapshotWriter::Write(std::ostream& stream, const RegistrySnapshot& snapshot) const
{
  Json keys = Json::array();

  for (const RegistryKey& key : snapshot.keys)
  {
    Json values = Json::array();

    for (const RegistryValue& value : key.values)
    {
      values.push_back({ { "name", value.name },
                         { "type", value.type },
                         { "data", ToHexadecimal(value.data) } });
    }

    keys.push_back({ { "path", key.path }, { "values", std::move(values) } });
  }

  Json document;
  document["schemaVersion"] = snapshot.schema_version;
  document["capturedAt"] = snapshot.captured_at;
  document["computerName"] = snapshot.computer_name;
  document["roots"] = snapshot.roots;
  document["diagnostics"] = snapshot.diagnostics;
  document["keys"] = std::move(keys);

  // Written compactly. A machine snapshot is large, and this is an interchange
  // format rather than something to read: any JSON tool will lay it out.
  stream << document.dump() << '\n';
}

RegistrySnapshot SnapshotReader::Read(std::istream& stream) const
{
  Json document;

  try
  {
    stream >> document;
  }
  catch (const Json::exception& error)
  {
    throw RegistryError(std::string("the snapshot is not valid JSON: ") + error.what());
  }

  if (!document.is_object())
  {
    throw RegistryError("the snapshot is not a JSON object");
  }

  ReadSchemaVersion(document);

  RegistrySnapshot snapshot;
  snapshot.captured_at = ReadString(document, "capturedAt");
  snapshot.computer_name = ReadString(document, "computerName");
  snapshot.roots = ReadStrings(document, "roots");
  snapshot.diagnostics = ReadStrings(document, "diagnostics");

  for (const Json& key : ReadArray(document, "keys"))
  {
    snapshot.keys.push_back(ReadKey(key));
  }

  Canonicalise(snapshot);
  return snapshot;
}
