#include "Regdiff/RegistryValueFormatter.h"

// Platform
#include "Platform/WindowsApi.h"

// Standard
#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace
{
  constexpr std::size_t kMaximumBytesShown = 32;
  constexpr std::size_t kMaximumCharactersShown = 100;

  std::string DescribeHexData(const std::vector<std::uint8_t>& data)
  {
    if (data.empty())
    {
      return "(empty)";
    }

    constexpr char digits[] = "0123456789abcdef";

    const std::size_t shown = std::min(data.size(), kMaximumBytesShown);

    std::string text;
    text.reserve(shown * 2);

    for (std::uint8_t byte : std::span(data.data(), shown))
    {
      text.push_back(digits[byte >> 4]);
      text.push_back(digits[byte & 0x0F]);
    }

    if (shown != data.size())
    {
      text += "... (" + std::to_string(data.size()) + " bytes)";
    }

    return text;
  }

  std::optional<std::uint32_t> ReadDword(
    const std::vector<std::uint8_t>& data)
  {
    if (data.size() != 4)
    {
      return std::nullopt;
    }

    return
      static_cast<std::uint32_t>(data[0]) |
      (static_cast<std::uint32_t>(data[1]) << 8) |
      (static_cast<std::uint32_t>(data[2]) << 16) |
      (static_cast<std::uint32_t>(data[3]) << 24);
  }

  std::optional<std::uint32_t> ReadBigEndianDword(
    const std::vector<std::uint8_t>& data)
  {
    if (data.size() != 4)
    {
      return std::nullopt;
    }

    return
      (static_cast<std::uint32_t>(data[0]) << 24) |
      (static_cast<std::uint32_t>(data[1]) << 16) |
      (static_cast<std::uint32_t>(data[2]) << 8) |
      static_cast<std::uint32_t>(data[3]);
  }

  std::optional<std::uint64_t> ReadQword(
    const std::vector<std::uint8_t>& data)
  {
    if (data.size() != 8)
    {
      return std::nullopt;
    }

    std::uint64_t value = 0;

    for (std::size_t i = 0; i < 8; ++i)
    {
      value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
    }

    return value;
  }

  std::string Truncate(std::string text)
  {
    if (text.size() <= kMaximumCharactersShown)
    {
      return text;
    }

    return text.substr(0, kMaximumCharactersShown)
      + "... (" + std::to_string(text.size()) + " characters)";
  }

  std::optional<std::string> DecodeString(
    const std::vector<std::uint8_t>& data)
  {
    if (data.empty())
    {
      return std::string{};
    }

    if (data.size() % 2 != 0)
    {
      return std::nullopt;
    }

    std::wstring wide;
    wide.reserve(data.size() / 2);

    for (std::size_t i = 0; i < data.size(); i += 2)
    {
      wchar_t ch =
        static_cast<wchar_t>(
          data[i] |
          (static_cast<std::uint16_t>(data[i + 1]) << 8));

      if (ch == L'\0')
      {
        break;
      }

      wide.push_back(ch);
    }

    if (wide.empty())
    {
      return std::string{};
    }

    const int required =
      WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required <= 0)
    {
      return std::nullopt;
    }

    std::string utf8(required, '\0');

    WideCharToMultiByte(
      CP_UTF8,
      0,
      wide.c_str(),
      static_cast<int>(wide.size()),
      utf8.data(),
      required,
      nullptr,
      nullptr);

    return Truncate(std::move(utf8));
  }
}

std::string FormatRegistryValue(const RegistryValue& value)
{
  switch (value.type)
  {
  case REG_SZ:
  case REG_EXPAND_SZ:
  {
    if (auto text = DecodeString(value.data))
    {
      return '"' + *text + '"';
    }
    break;
  }

  case REG_DWORD:
  {
    if (auto number = ReadDword(value.data))
    {
      return std::format("{} (0x{:08X})", *number, *number);
    }
    break;
  }

  case REG_DWORD_BIG_ENDIAN:
  {
    if (auto number = ReadBigEndianDword(value.data))
    {
      return std::format("{} (0x{:08X})", *number, *number);
    }
    break;
  }

  case REG_QWORD:
  {
    if (auto number = ReadQword(value.data))
    {
      return std::format("{} (0x{:016X})", *number, *number);
    }
    break;
  }

  default:
    break;
  }

  return DescribeHexData(value.data);
}