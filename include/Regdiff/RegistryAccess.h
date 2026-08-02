#pragma once

// Project
#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <string>
#include <string_view>
#include <vector>

// What one Registry Key contains, or why it could not be read.
struct RegistryReadResult
{
  bool opened = false;
  std::string error;  // Why the Registry Key could not be opened. Set when opened is false.

  std::vector<std::string> subkey_names;
  std::vector<RegistryValue> values;
};

// Read-only access to a Windows Registry.
//
// RegistrySnapshotter works through this interface so that its traversal can be
// exercised against an in-memory Registry in the unit tests. That is the only
// reason the interface exists, and it is the only abstract class in RegDiff.
class RegistryAccess
{
public:
  virtual ~RegistryAccess() = default;

  // Reads the subkeys and the Registry Values of one Registry Key.
  virtual RegistryReadResult Read(std::string_view path) const = 0;
};

// Reads the live Windows Registry through the Win32 Registry API.
//
// Registry Keys are opened with KEY_READ and nothing else. RegDiff never
// modifies the Registry.
class Win32RegistryAccess : public RegistryAccess
{
public:
  RegistryReadResult Read(std::string_view path) const override;
};

// The name of the machine RegDiff is running on.
//
// Recorded in a snapshot so that one taken on another machine can be told
// apart. It lives here because this is where RegDiff talks to Windows.
std::string LocalComputerName();
