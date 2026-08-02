#pragma once

// Standard
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The version of the JSON snapshot format this build reads and writes.
//
// Increment it whenever a change would stop an older build of RegDiff reading a
// snapshot correctly.
inline constexpr int kSnapshotSchemaVersion = 1;

// One Registry Value, exactly as Windows reported it.
//
// The data is kept as raw bytes and the type as the number Windows uses, so a
// Registry Value Type RegDiff has never heard of still survives a round trip
// through a snapshot and is still compared reliably.
struct RegistryValue
{
  std::string name;               // UTF-8. Empty for the default Registry Value.
  std::uint32_t type = 0;         // REG_SZ, REG_DWORD, and so on, as reported by Windows.
  std::vector<std::uint8_t> data; // Registry Value Data exactly as returned by Windows.
};

struct RegistryKey
{
  std::string path;                   // UTF-8, for example "HKEY_CURRENT_USER\\Software".
  std::vector<RegistryValue> values;  // In canonical order.
};

struct RegistrySnapshot
{
  int schema_version = kSnapshotSchemaVersion;
  std::string captured_at;             // ISO 8601 in UTC, for example "2026-08-02T09:15:00Z".
  std::string computer_name;
  std::vector<std::string> roots;        // The Registry Keys the snapshot was taken from.
  std::vector<std::string> diagnostics;  // Registry Keys that could not be read, and why.
  std::vector<RegistryKey> keys;         // In canonical order.
};

// Orders two Registry Key paths or two Value Names.
//
// Windows treats both as case-insensitive, so RegDiff does too, otherwise a
// program that rewrote a name in a different case would look like a change.
// Only ASCII letters are folded, which leaves UTF-8 sequences untouched: two
// names differing only in the case of a non-ASCII letter are reported as
// different, which is the safe way round for a tool whose job is to notice
// changes.
//
// Returns a negative number, zero, or a positive number, like std::strcmp.
int CompareRegistryNames(std::string_view left, std::string_view right);

// Puts a snapshot into canonical order: Registry Keys sorted by path, Registry
// Values sorted by Value Name, and each Registry Key listed once.
//
// The comparator walks two snapshots in step, so both have to be in this order.
// Overlapping roots, for example HKLM\SOFTWARE and HKLM\SOFTWARE\Microsoft,
// reach the same Registry Key twice, and a snapshot written by hand need not
// have been ordered at all.
void Canonicalise(RegistrySnapshot& snapshot);

// Formats bytes as lowercase hexadecimal.
//
// This is how Registry Value Data is written to a snapshot and shown in a
// report, so both spell it the same way.
std::string ToHexadecimal(std::span<const std::uint8_t> data);

// The standard Windows Registry hives, by their full names, in the order the
// snapshot command captures them when it is asked for all of them.
std::vector<std::string> StandardRegistryRoots();

// Rewrites the Registry root at the front of a path to its full name, so that
// "HKLM\SOFTWARE", "hklm\Software", and "HKEY_LOCAL_MACHINE\SOFTWARE" all
// become one spelling and snapshots taken with any of them compare equal.
//
// A path that does not begin with a Registry root RegDiff knows is returned
// unchanged, and is reported by whatever then fails to read it.
std::string ExpandRegistryRoot(std::string_view path);
