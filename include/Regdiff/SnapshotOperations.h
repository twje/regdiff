#pragma once

// Project
#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <filesystem>
#include <vector>

// Capture the current Registry into a snapshot.
//
// The returned snapshot contains the capture timestamp and computer name.
// Throws RegistryError if no Registry Keys could be captured.
RegistrySnapshot CaptureSnapshot(const std::vector<std::string>& roots);

// Capture and write a snapshot to disk.
//
// Returns the captured snapshot so callers can immediately display it.
// Throws RegistryError on failure.
RegistrySnapshot SaveSnapshot(
  const std::vector<std::string>& roots,
  const std::filesystem::path& file);

// Load a snapshot from disk.
//
// Throws RegistryError if the snapshot cannot be read.
RegistrySnapshot LoadSnapshot(
  const std::filesystem::path& file);